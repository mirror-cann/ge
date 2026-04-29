/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/custom_op_factory_impl.h"

#include "debug/ge_log.h"
#include "common/util/mem_utils.h"
#include "framework/common/types.h"

namespace ge {
Status CustomOpFactoryImpl::RegisterCustomOpCreator(const AscendString &op_type, const BaseOpCreator &op_creator) {
  const std::lock_guard<std::mutex> lock(mu_);
  if (op_creator == nullptr) {
    GELOGE(GRAPH_PARAM_INVALID, "[Check][Param] custom op creator for %s is null.", op_type.GetString());
    return GRAPH_PARAM_INVALID;
  }
  const auto it = custom_op_creators_.find(op_type);
  if (it != custom_op_creators_.cend()) {
    GELOGW("[CUSTOM OP] custom op creator for %s already exist.", op_type.GetString());
    return GRAPH_FAILED;
  }
  (void)custom_op_creators_.emplace(op_type, op_creator);
  GELOGI("[CUSTOM OP] register custom operator creator for %s.", op_type.GetString());
  return GRAPH_SUCCESS;
}

BaseCustomOp *CustomOpFactoryImpl::CreateOrGetCustomOp(const AscendString &op_type) {
  const std::lock_guard<std::mutex> lock(mu_);
  if (const auto it = custom_ops_.find(op_type); it != custom_ops_.cend()) {
    GELOGD("[CUSTOM OP] custom_op %s already created .", op_type.GetString());
    return it->second.get();
  }
  if (const auto op_creator_it = custom_op_creators_.find(op_type); op_creator_it != custom_op_creators_.cend()) {
    if (op_creator_it->second == nullptr) {
      GELOGE(GRAPH_PARAM_INVALID, "[Check][Param] custom op creator for %s is null.", op_type.GetString());
      return nullptr;
    }
    auto base_custom_op = op_creator_it->second();
    auto [ops_it, success] = custom_ops_.emplace(op_type, std::move(base_custom_op));
    if (success) {
      return ops_it->second.get();
    }
    GELOGW("[CUSTOM OP] custom op instance found, get failed for %s.", op_type.GetString());
  }
  GELOGW("[CUSTOM OP] get custom operator creator failed for %s.", op_type.GetString());
  return nullptr;
}

Status CustomOpFactoryImpl::GetAllRegisteredOps(std::vector<AscendString> &all_registered_ops) {
  const std::lock_guard<std::mutex> lock(mu_);
  for (const auto &op_creator : custom_op_creators_) {
    all_registered_ops.push_back(op_creator.first);
  }
  return GRAPH_SUCCESS;
}

bool CustomOpFactoryImpl::IsExistOp(const AscendString &op_type) {
  const std::lock_guard<std::mutex> lock(mu_);
  return custom_op_creators_.find(op_type) != custom_op_creators_.end();
}

graphStatus CustomOpFactoryImpl::LoadCustomOpsPartition(const uint8_t *data, size_t len) {
  if ((data == nullptr) || (len == 0U)) {
    GELOGE(GRAPH_PARAM_INVALID, "[CUSTOM OP] custom ops partition data is invalid, data %p, len %zu.", data, len);
    return GRAPH_PARAM_INVALID;
  }

  size_t offset = 0U;
  while (offset < len) {
    // 1. 检查剩余空间是否足够读取 header
    if (offset + sizeof(CustomKernelItemHeader) > len) {
      GELOGE(GRAPH_FAILED, "[CUSTOM OP] Insufficient data for CustomKernelItemHeader at offset %zu", offset);
      return GRAPH_FAILED;
    }

    // 2. 解析 header
    const auto *header = reinterpret_cast<const CustomKernelItemHeader *>(data + offset);
    if (header->magic != kCustomKernelItemMagic) {
      GELOGE(GRAPH_FAILED, "[CUSTOM OP] Invalid magic in CustomKernelItemHeader: 0x%X, expected 0x%X",
             header->magic, kCustomKernelItemMagic);
      return GRAPH_FAILED;
    }

    // 3. 检查剩余空间是否足够读取 name + bin
    const size_t entry_size = sizeof(CustomKernelItemHeader) + static_cast<size_t>(header->name_len) +
                              static_cast<size_t>(header->bin_len);
    if (offset + entry_size > len) {
      GELOGE(GRAPH_FAILED, "[CUSTOM OP] Insufficient data for kernel entry at offset %zu, need %zu bytes", offset, entry_size);
      return GRAPH_FAILED;
    }

    // 4. 读取 op_type
    const char *op_type_ptr = reinterpret_cast<const char *>(data + offset + sizeof(CustomKernelItemHeader));
    const std::string op_type(op_type_ptr, header->name_len);

    // 5. 创建算子实例
    auto op = CreateOrGetCustomOp(AscendString(op_type.c_str()));
    if (op == nullptr) {
      GELOGE(GRAPH_FAILED, "[CUSTOM OP] Custom op '%s' not found in registry, "
             "environment mismatch detected", op_type.c_str());
      return GRAPH_FAILED;
    }

    // 6. 能力检测
    auto *serializable_op = dynamic_cast<PortableOp *>(op);
    if (serializable_op == nullptr) {
      GELOGE(GRAPH_FAILED, "[CUSTOM OP] Custom op '%s' is not PortableOp, "
             "type mismatch or version inconsistency detected", op_type.c_str());
      return GRAPH_FAILED;
    }

    // 7. 调用反序列化
    const uint8_t *kernel_bin = data + offset + sizeof(CustomKernelItemHeader) + header->name_len;
    const std::vector<uint8_t> kernel_bin_buffer(kernel_bin, kernel_bin + header->bin_len);
    const auto ret = serializable_op->Deserialize(kernel_bin_buffer);
    if (ret != GRAPH_SUCCESS) {
      GELOGE(ret, "[CUSTOM OP] Failed to deserialize custom op '%s'", op_type.c_str());
      return ret;
    }

    GELOGI("[CUSTOM OP] Successfully deserialized custom op '%s'", op_type.c_str());

    // 8. 推进 offset
    offset += entry_size;
  }

  GELOGI("[CUSTOM OP] load custom ops partition success.");
  return GRAPH_SUCCESS;
}

CustomOpFactoryImpl::CustomOpFactoryImpl() = default;
} // namespace ge
