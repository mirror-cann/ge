/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/custom_op.h"
#include "graph/custom_op_registry.h"

#include <limits>
#include <string>
#include <utility>

#include "debug/ge_log.h"
#include "framework/common/framework_types_internal.h"
#include "graph/custom_op/cast.h"
#include "graph/operator_factory_impl.h"

namespace ge {
namespace {
struct ParsedCustomKernelItem {
  std::string op_type;
  const uint8_t *kernel_bin;
  size_t bin_len;
  size_t entry_size;
};

graphStatus ParseCustomKernelItem(const uint8_t *data, const size_t len, const size_t offset,
                                  ParsedCustomKernelItem &item) {
  const size_t header_size = sizeof(CustomKernelItemHeader);
  if (header_size > (len - offset)) {
    GELOGE(GRAPH_FAILED, "[CUSTOM OP] Insufficient data for CustomKernelItemHeader at offset %zu", offset);
    return GRAPH_FAILED;
  }

  const auto *header = reinterpret_cast<const CustomKernelItemHeader *>(data + offset);
  if (header->magic != kCustomKernelItemMagic) {
    GELOGE(GRAPH_FAILED, "[CUSTOM OP] Invalid magic in CustomKernelItemHeader: 0x%X, expected 0x%X", header->magic,
           kCustomKernelItemMagic);
    return GRAPH_FAILED;
  }

  const size_t name_len = static_cast<size_t>(header->name_len);
  const size_t bin_len = static_cast<size_t>(header->bin_len);
  if ((name_len > (std::numeric_limits<size_t>::max() - header_size)) ||
      (bin_len > (std::numeric_limits<size_t>::max() - header_size - name_len))) {
    GELOGE(GRAPH_FAILED, "[CUSTOM OP] Invalid kernel entry size at offset %zu, name len %zu, bin len %zu", offset,
           name_len, bin_len);
    return GRAPH_FAILED;
  }

  const size_t entry_size = header_size + name_len + bin_len;
  if (entry_size > (len - offset)) {
    GELOGE(GRAPH_FAILED, "[CUSTOM OP] Insufficient data for kernel entry at offset %zu, need %zu bytes", offset,
           entry_size);
    return GRAPH_FAILED;
  }

  const char *op_type_ptr = reinterpret_cast<const char *>(data + offset + header_size);
  item.op_type = std::string(op_type_ptr, name_len);
  item.kernel_bin = data + offset + header_size + name_len;
  item.bin_len = bin_len;
  item.entry_size = entry_size;
  return GRAPH_SUCCESS;
}

graphStatus DeserializeCustomKernelItem(CustomOpRegistry &registry, const ParsedCustomKernelItem &item) {
  auto op = registry.CreateOrGetCustomOp(AscendString(item.op_type.c_str()));
  if (op == nullptr) {
    GELOGE(GRAPH_FAILED, "[CUSTOM OP] Custom op '%s' not found in registry, environment mismatch detected",
           item.op_type.c_str());
    return GRAPH_FAILED;
  }

  auto *serializable_op = CustomOpCast<PortableOp>(op);
  if (serializable_op == nullptr) {
    GELOGE(GRAPH_FAILED,
           "[CUSTOM OP] Custom op '%s' is not PortableOp, type mismatch or version inconsistency detected",
           item.op_type.c_str());
    return GRAPH_FAILED;
  }

  const std::vector<uint8_t> kernel_bin_buffer(item.kernel_bin, item.kernel_bin + item.bin_len);
  const auto ret = serializable_op->Deserialize(kernel_bin_buffer);
  if (ret != GRAPH_SUCCESS) {
    GELOGE(ret, "[CUSTOM OP] Failed to deserialize custom op '%s'", item.op_type.c_str());
    return ret;
  }

  GELOGI("[CUSTOM OP] Successfully deserialized custom op '%s'", item.op_type.c_str());
  return GRAPH_SUCCESS;
}
}  // namespace

CustomOpRegistry::~CustomOpRegistry() {
  try {
    std::vector<std::string> op_types;
    {
      const std::lock_guard<std::mutex> lock(mu_);
      for (const auto &entry : creators_) {
        op_types.push_back(entry.first.GetString());
      }
    }
    if (!op_types.empty()) {
      OperatorFactoryImpl::RemoveCustomOpCreators(op_types);
    }
  } catch (const std::exception &e) {
    GELOGW("[CUSTOM OP] Exception in CustomOpRegistry destructor: %s", e.what());
  } catch (...) {
    GELOGW("[CUSTOM OP] Unknown exception in CustomOpRegistry destructor.");
  }
}

graphStatus CustomOpRegistry::RegisterCreator(const AscendString &op_type, const BaseOpCreator &creator) {
  const std::lock_guard<std::mutex> lock(mu_);
  if (creator == nullptr) {
    GELOGE(GRAPH_PARAM_INVALID, "[Check][Param] custom op creator for %s is null.", op_type.GetString());
    return GRAPH_PARAM_INVALID;
  }
  const auto it = creators_.find(op_type);
  if (it != creators_.cend()) {
    GELOGW("[CUSTOM OP] custom op creator for %s already exist.", op_type.GetString());
    return GRAPH_FAILED;
  }
  (void)creators_.emplace(op_type, creator);
  GELOGI("[CUSTOM OP] register custom operator creator for %s.", op_type.GetString());
  return GRAPH_SUCCESS;
}

void CustomOpRegistry::AddSoHandles(const std::vector<CustomOpSoHandlePtr> &so_handles) {
  const std::lock_guard<std::mutex> lock(mu_);
  so_handles_.insert(so_handles_.end(), so_handles.begin(), so_handles.end());
}

BaseCustomOp *CustomOpRegistry::CreateOrGetCustomOp(const AscendString &op_type) {
  const std::lock_guard<std::mutex> lock(mu_);
  if (const auto it = custom_ops_.find(op_type); it != custom_ops_.cend()) {
    GELOGD("[CUSTOM OP] custom_op %s already created .", op_type.GetString());
    return it->second.get();
  }
  if (const auto op_creator_it = creators_.find(op_type); op_creator_it != creators_.cend()) {
    if (op_creator_it->second == nullptr) {
      GELOGE(GRAPH_PARAM_INVALID, "[Check][Param] custom op creator for %s is null.", op_type.GetString());
      return nullptr;
    }
    auto base_custom_op = op_creator_it->second();
    auto [ops_it, success] = custom_ops_.emplace(op_type, std::shared_ptr<BaseCustomOp>(std::move(base_custom_op)));
    if (success) {
      return ops_it->second.get();
    }
    GELOGW("[CUSTOM OP] custom op instance found, get failed for %s.", op_type.GetString());
  }
  GELOGW("[CUSTOM OP] get custom operator creator failed for %s.", op_type.GetString());
  return nullptr;
}

void CustomOpRegistry::RemoveCustomOps(const std::vector<AscendString> &op_types) {
  std::vector<std::shared_ptr<BaseCustomOp>> removed_custom_ops;
  {
    const std::lock_guard<std::mutex> lock(mu_);
    for (const auto &op_type : op_types) {
      const auto custom_op_iter = custom_ops_.find(op_type);
      if (custom_op_iter != custom_ops_.cend()) {
        removed_custom_ops.emplace_back(std::move(custom_op_iter->second));
        (void)custom_ops_.erase(custom_op_iter);
      }

      const auto creator_iter = creators_.find(op_type);
      if (creator_iter != creators_.cend()) {
        (void)creators_.erase(creator_iter);
      }
    }
  }

  removed_custom_ops.clear();
}

ArgsRefreshStrategy CustomOpRegistry::GetArgsRefreshStrategy(const AscendString &op_type) {
  const auto *custom_op = CreateOrGetCustomOp(op_type);
  if (custom_op == nullptr) {
    return ArgsRefreshStrategy::kNone;
  }
  if (CustomOpCast<ArgsUpdater>(custom_op) != nullptr) {
    return ArgsRefreshStrategy::kUpdateCallback;
  }
  if (CustomOpCast<AnnotatedArgsOp>(custom_op) != nullptr) {
    return ArgsRefreshStrategy::kAnnotatedArgs;
  }
  return ArgsRefreshStrategy::kNone;
}

bool CustomOpRegistry::IsAddressRefreshable(const AscendString &op_type) {
  return GetArgsRefreshStrategy(op_type) != ArgsRefreshStrategy::kNone;
}

BaseCustomOp *CustomOpRegistry::FindCustomOp(const AscendString &op_type) const {
  const std::lock_guard<std::mutex> lock(mu_);
  const auto it = custom_ops_.find(op_type);
  return (it == custom_ops_.cend()) ? nullptr : it->second.get();
}

bool CustomOpRegistry::HasCreator(const AscendString &op_type) const {
  const std::lock_guard<std::mutex> lock(mu_);
  return creators_.find(op_type) != creators_.cend();
}

bool CustomOpRegistry::HasCustomOp(const AscendString &op_type) const {
  const std::lock_guard<std::mutex> lock(mu_);
  return custom_ops_.find(op_type) != custom_ops_.cend();
}

graphStatus CustomOpRegistry::GetAllRegisteredOps(std::vector<AscendString> &all_registered_ops) const {
  const std::lock_guard<std::mutex> lock(mu_);
  for (const auto &op_creator : creators_) {
    all_registered_ops.push_back(op_creator.first);
  }
  return GRAPH_SUCCESS;
}

graphStatus CustomOpRegistry::LoadCustomOpsPartition(const uint8_t *data, size_t len) {
  if ((data == nullptr) || (len == 0U)) {
    GELOGE(GRAPH_PARAM_INVALID, "[CUSTOM OP] custom ops partition data is invalid, data %p, len %zu.", data, len);
    return GRAPH_PARAM_INVALID;
  }

  size_t offset = 0U;
  while (offset < len) {
    ParsedCustomKernelItem item{};
    const auto parse_ret = ParseCustomKernelItem(data, len, offset, item);
    if (parse_ret != GRAPH_SUCCESS) {
      return parse_ret;
    }
    const auto deserialize_ret = DeserializeCustomKernelItem(*this, item);
    if (deserialize_ret != GRAPH_SUCCESS) {
      return deserialize_ret;
    }
    offset += item.entry_size;
  }

  GELOGI("[CUSTOM OP] load custom ops partition success.");
  return GRAPH_SUCCESS;
}
}  // namespace ge
