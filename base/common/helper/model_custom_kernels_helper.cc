/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/common/helper/model_helper.h"
#include "external/graph/custom_op.h"
#include "graph/custom_op_factory.h"

namespace ge {
Status ModelHelper::CollectUsedCustomOpTypes(const GeRootModelPtr &ge_root_model,
                                             std::set<std::string> &used_custom_op_types) const {
  if (ge_root_model->GetRootGraph() != nullptr) {
    const auto &root_graph = ge_root_model->GetRootGraph();
    for (const auto &node : root_graph->GetAllNodes()) {
      const std::string op_type = node->GetType();
      if (CustomOpFactory::IsExistOp(AscendString(op_type.c_str()))) {
        used_custom_op_types.insert(op_type);
      }
    }
  }

  // subgraph_instance_name_to_model_ 中的 GeModel 可能持有独立的 ComputeGraph 对象，
  // 这些子图未必通过 AddSubgraph 挂入 root_graph 的子图树，因此无法被上方 GetAllNodes() 遍历到。
  // 典型场景：编译分区后各分区独立持有自己的 ComputeGraph，或反序列化时每个 GeModel 单独还原图对象。
  // 此处需额外遍历，以确保这类游离子图中的自定义算子也被收集到。
  const auto &subgraph_map = ge_root_model->GetSubgraphInstanceNameToModel();
  for (const auto &subgraph_pair : subgraph_map) {
    const auto &ge_model = subgraph_pair.second;
    if (ge_model == nullptr || ge_model->GetGraph() == nullptr) {
      continue;
    }
    const auto &graph = ge_model->GetGraph();
    if (graph == ge_root_model->GetRootGraph()) {
      continue;
    }
    for (const auto &node : graph->GetAllNodes()) {
      const std::string op_type = node->GetType();
      if (CustomOpFactory::IsExistOp(AscendString(op_type.c_str()))) {
        used_custom_op_types.insert(op_type);
      }
    }
  }
  return SUCCESS;
}

Status ModelHelper::SerializeCustomOpKernel(PortableOp *serializable_op, const std::string &op_type_str,
                                            std::vector<uint8_t> &merged_buffers) {
  if (serializable_op == nullptr) {
    GELOGE(FAILED, "[CUSTOM OP] serializable custom op is null, op_type:%s", op_type_str.c_str());
    return FAILED;
  }

  std::vector<uint8_t> buffer;
  const auto ret = serializable_op->Serialize(buffer);
  if (ret != GRAPH_SUCCESS) {
    GELOGE(ret, "[CUSTOM OP] serialize failed, op_type:%s", op_type_str.c_str());
    return ret;
  }

  if (buffer.empty()) {
    GELOGW("[CUSTOM OP] serialized buffer is empty, skip, op_type:%s", op_type_str.c_str());
    return SUCCESS;
  }

  CustomKernelItemHeader header;
  header.magic = kCustomKernelItemMagic;
  header.name_len = static_cast<uint32_t>(op_type_str.size());
  header.bin_len = static_cast<uint32_t>(buffer.size());

  const auto *header_ptr = reinterpret_cast<const uint8_t *>(&header);
  merged_buffers.insert(merged_buffers.end(), header_ptr, header_ptr + sizeof(header));
  merged_buffers.insert(merged_buffers.end(), op_type_str.begin(), op_type_str.end());
  merged_buffers.insert(merged_buffers.end(), buffer.begin(), buffer.end());
  GELOGD("[CUSTOM OP] Serialized custom op '%s', bin size:%zu", op_type_str.c_str(), buffer.size());
  return SUCCESS;
}

Status ModelHelper::SaveCustomOpsPartition(std::shared_ptr<OmFileSaveHelper> &om_file_save_helper,
                                               const GeRootModelPtr &ge_root_model) {
  std::set<std::string> used_custom_op_types;
  GE_ASSERT_SUCCESS(CollectUsedCustomOpTypes(ge_root_model, used_custom_op_types));

  if (used_custom_op_types.empty()) {
    GELOGI("[CUSTOM OP] No custom ops used in graph, skip saving custom kernels partition.");
    return SUCCESS;
  }

  bool has_serializable_custom_op = false;
  bool has_non_serializable_custom_op = false;
  std::vector<std::pair<std::string, PortableOp *>> serializable_ops;
  std::vector<std::unique_ptr<BaseCustomOp>> serializable_op_owners;
  serializable_ops.reserve(used_custom_op_types.size());
  serializable_op_owners.reserve(used_custom_op_types.size());
  for (const auto &op_type_str : used_custom_op_types) {
    auto op = CustomOpFactory::CreateCustomOp(AscendString(op_type_str.c_str()));
    if (op == nullptr) {
      GELOGE(FAILED, "[CUSTOM OP] create custom op failed, op_type:%s", op_type_str.c_str());
      return FAILED;
    }
    auto *serializable_op = dynamic_cast<PortableOp *>(op.get());
    if (serializable_op == nullptr) {
      has_non_serializable_custom_op = true;
    } else {
      has_serializable_custom_op = true;
      serializable_ops.emplace_back(op_type_str, serializable_op);
      serializable_op_owners.emplace_back(std::move(op));
    }
    if (has_serializable_custom_op && has_non_serializable_custom_op) {
      GELOGE(FAILED,
             "[CUSTOM OP] graph contains both serializable and non-serializable custom ops.");
      return FAILED;
    }
  }

  std::vector<uint8_t> merged_buffers;
  for (const auto &serializable_op : serializable_ops) {
    GE_ASSERT_SUCCESS(SerializeCustomOpKernel(serializable_op.second, serializable_op.first, merged_buffers));
  }

  if (merged_buffers.empty()) {
    GELOGI("[CUSTOM OP] no custom ops serialized, skip saving custom_ops partition.");
    return SUCCESS;
  }

  GELOGI("[CUSTOM OP] custom ops partition size:%zu", merged_buffers.size());
  return om_file_save_helper->AddOwnedPartition(ModelPartitionType::CUSTOM_OPS, std::move(merged_buffers), 0U);
}

Status ModelHelper::LoadCustomOps(const OmFileLoadHelper &om_load_helper) const {
  ModelPartition custom_ops_partition;
  if (om_load_helper.GetModelPartition(ModelPartitionType::CUSTOM_OPS, custom_ops_partition, 0U) != SUCCESS) {
    GELOGI("[CUSTOM OP] custom ops partition not found, skip load.");
    return SUCCESS;
  }

  if ((custom_ops_partition.data == nullptr) || (custom_ops_partition.size == 0U)) {
    GELOGI("[CUSTOM OP] custom ops partition is empty, skip load.");
    return SUCCESS;
  }

  GE_CHK_STATUS_RET(CustomOpFactory::LoadCustomOpsPartition(custom_ops_partition.data,
                                                                 custom_ops_partition.size),
                    "[CUSTOM OP] Load custom ops partition failed.");
  return SUCCESS;
}
}  // namespace ge
