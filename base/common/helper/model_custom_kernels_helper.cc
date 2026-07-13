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

#include <cinttypes>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "common/helper/custom_op_registry_builder.h"
#include "common/helper/custom_op_so_loader.h"
#include "common/model/ge_root_model.h"
#include "external/graph/custom_op.h"
#include "graph/custom_op/cast.h"
#include "graph/custom_op_registry.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/attr_utils.h"

namespace ge {
namespace {
Status ValidateCustomOpNodeDeserialized(const NodePtr &node, const CustomOpRegistryPtr &registry) {
  if (node == nullptr) {
    return SUCCESS;
  }

  const std::string op_type = node->GetType();
  const AscendString ascend_op_type(op_type.c_str());
  if (!registry->HasCreator(ascend_op_type)) {
    GELOGD("[CUSTOM OP] op %s is not found in registry.", op_type.c_str());
    return SUCCESS;
  }

  if (!registry->HasCustomOp(ascend_op_type)) {
    GELOGE(FAILED, "[CUSTOM OP] custom op %s is used by model but not deserialized.", op_type.c_str());
    return FAILED;
  }
  return SUCCESS;
}

Status ValidateCustomOpsInGraphDeserialized(const ComputeGraphPtr &graph, const CustomOpRegistryPtr &registry,
                                            std::set<ComputeGraph *> &visited_graphs) {
  if (graph == nullptr) {
    return SUCCESS;
  }
  if (!visited_graphs.insert(graph.get()).second) {
    return SUCCESS;
  }

  for (const auto &node : graph->GetAllNodes()) {
    GE_ASSERT_SUCCESS(ValidateCustomOpNodeDeserialized(node, registry));
  }
  return SUCCESS;
}

bool HasNonEmptyCustomOpsPartition(const OmFileLoadHelper &om_load_helper) {
  ModelPartition custom_ops_partition;
  if (om_load_helper.GetModelPartition(ModelPartitionType::CUSTOM_OPS, custom_ops_partition, 0U) != SUCCESS) {
    return false;
  }
  return (custom_ops_partition.data != nullptr) && (custom_ops_partition.size > 0U);
}

Status CollectCustomOpTypesFromGraph(const ComputeGraphPtr &graph, const CustomOpRegistryPtr &registry,
                                     std::set<std::string> &used_custom_op_types) {
  if (graph == nullptr) {
    return SUCCESS;
  }
  GE_ASSERT_NOTNULL(registry);
  for (const auto &node : graph->GetAllNodes()) {
    const std::string op_type = node->GetType();
    if (registry->HasCreator(AscendString(op_type.c_str()))) {
      (void)used_custom_op_types.insert(op_type);
    }
  }
  return SUCCESS;
}
}  // namespace

Status LoadCustomOpsToRegistry(const uint8_t *data, const size_t len, const CustomOpRegistryPtr &registry) {
  if (registry == nullptr) {
    GELOGE(FAILED, "[CUSTOM OP] model custom op registry is null.");
    return FAILED;
  }
  GE_CHK_STATUS_RET(registry->LoadCustomOpsPartition(data, len),
                    "[CUSTOM OP] Load custom ops partition to model registry failed.");
  return SUCCESS;
}

Status ModelHelper::CollectUsedCustomOpTypes(const GeRootModelPtr &ge_root_model,
                                             std::set<std::string> &used_custom_op_types) const {
  GE_ASSERT_NOTNULL(ge_root_model);
  const auto &registry = ge_root_model->GetCustomOpRegistry();
  GE_ASSERT_NOTNULL(registry);
  GE_ASSERT_SUCCESS(CollectCustomOpTypesFromGraph(ge_root_model->GetRootGraph(), registry, used_custom_op_types));

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
    GE_ASSERT_SUCCESS(CollectCustomOpTypesFromGraph(graph, registry, used_custom_op_types));
  }
  return SUCCESS;
}

Status ModelHelper::SerializeCustomOpKernel(PortableOp *serializable_op, const std::string &op_type_str,
                                            std::vector<uint8_t> &merged_buffers) const {
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
    GELOGE(FAILED, "[CUSTOM OP] serialized buffer is empty, op_type:%s", op_type_str.c_str());
    return FAILED;
  }

  CustomKernelItemHeader header;
  header.magic = kCustomKernelItemMagic;
  header.name_len = static_cast<uint32_t>(op_type_str.size());
  header.bin_len = static_cast<uint32_t>(buffer.size());

  const auto *header_ptr = reinterpret_cast<const uint8_t *>(&header);
  (void)merged_buffers.insert(merged_buffers.end(), header_ptr, header_ptr + sizeof(header));
  (void)merged_buffers.insert(merged_buffers.end(), op_type_str.begin(), op_type_str.end());
  (void)merged_buffers.insert(merged_buffers.end(), buffer.begin(), buffer.end());
  GELOGD("[CUSTOM OP] Serialized custom op '%s', bin size:%zu", op_type_str.c_str(), buffer.size());
  return SUCCESS;
}

Status ModelHelper::SaveCustomOpsPartition(std::shared_ptr<OmFileSaveHelper> &om_file_save_helper,
                                           const GeRootModelPtr &ge_root_model) const {
  GE_ASSERT_NOTNULL(ge_root_model);
  const auto &registry = ge_root_model->GetCustomOpRegistry();
  if (registry == nullptr) {
    GELOGI("[CUSTOM OP] model custom op registry is null, skip saving custom ops partition.");
    return SUCCESS;
  }

  std::set<std::string> used_custom_op_types;
  GE_ASSERT_SUCCESS(CollectUsedCustomOpTypes(ge_root_model, used_custom_op_types));

  if (used_custom_op_types.empty()) {
    GELOGI("[CUSTOM OP] No custom ops used in graph, skip saving custom kernels partition.");
    return SUCCESS;
  }

  bool has_serializable_custom_op = false;
  bool has_non_serializable_custom_op = false;
  std::vector<std::pair<std::string, PortableOp *>> serializable_ops;
  serializable_ops.reserve(used_custom_op_types.size());
  for (const auto &op_type_str : used_custom_op_types) {
    auto op = registry->CreateOrGetCustomOp(AscendString(op_type_str.c_str()));
    if (op == nullptr) {
      GELOGE(FAILED, "[CUSTOM OP] create custom op failed, op_type:%s", op_type_str.c_str());
      return FAILED;
    }
    auto *serializable_op = CustomOpCast<PortableOp>(op);
    if (serializable_op == nullptr) {
      has_non_serializable_custom_op = true;
    } else {
      has_serializable_custom_op = true;
      (void)serializable_ops.emplace_back(op_type_str, serializable_op);
    }
    if (has_serializable_custom_op && has_non_serializable_custom_op) {
      GELOGE(FAILED, "[CUSTOM OP] graph contains both serializable and non-serializable custom ops.");
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

Status ModelHelper::ValidateCustomOpsDeserialized(const GeRootModelPtr &ge_root_model,
                                                  const CustomOpRegistryPtr &registry) const {
  GE_ASSERT_NOTNULL(ge_root_model);
  GE_ASSERT_NOTNULL(registry);

  std::set<ComputeGraph *> visited_graphs;
  GE_ASSERT_SUCCESS(ValidateCustomOpsInGraphDeserialized(ge_root_model->GetRootGraph(), registry, visited_graphs));

  const auto &subgraph_map = ge_root_model->GetSubgraphInstanceNameToModel();
  for (const auto &subgraph_pair : subgraph_map) {
    const auto &ge_model = subgraph_pair.second;
    if (ge_model == nullptr) {
      continue;
    }
    GE_ASSERT_SUCCESS(ValidateCustomOpsInGraphDeserialized(ge_model->GetGraph(), registry, visited_graphs));
  }
  return SUCCESS;
}

Status ModelHelper::LoadCustomOps(const OmFileLoadHelper &om_load_helper, const CustomOpRegistryPtr &registry) const {
  ModelPartition custom_ops_partition;
  if (om_load_helper.GetModelPartition(ModelPartitionType::CUSTOM_OPS, custom_ops_partition, 0U) != SUCCESS) {
    GELOGI("[CUSTOM OP] custom ops partition not found, skip load.");
    return SUCCESS;
  }

  if ((custom_ops_partition.data == nullptr) || (custom_ops_partition.size == 0U)) {
    GELOGI("[CUSTOM OP] custom ops partition is empty, skip load.");
    return SUCCESS;
  }

  GE_CHK_STATUS_RET(LoadCustomOpsToRegistry(custom_ops_partition.data, custom_ops_partition.size, registry),
                    "[CUSTOM OP] Load custom ops partition to model registry failed.");
  return SUCCESS;
}

Status ModelHelper::LoadCustomOpRegistry(const OmFileLoadHelper &om_load_helper,
                                         const GeRootModelPtr &ge_root_model) const {
  GE_ASSERT_NOTNULL(ge_root_model);
  std::vector<CustomOpSoHandlePtr> loaded_handles;
  GE_CHK_STATUS_RET(LoadOpSoBin(om_load_helper, ge_root_model, loaded_handles), "[CUSTOM OP] Load so bins failed.");
  auto registry = std::make_shared<CustomOpRegistry>();
  GE_ASSERT_NOTNULL(registry);
  if (loaded_handles.empty()) {
    GE_ASSERT_TRUE(!HasNonEmptyCustomOpsPartition(om_load_helper),
                   "[CUSTOM OP] custom ops partition exists but no custom op so is loaded.");
    ge_root_model->SetCustomOpRegistry(registry);
    GELOGI("[CUSTOM OP] no custom op so loaded, set empty model custom op registry.");
    return SUCCESS;
  }

  GE_CHK_STATUS_RET(CustomOpRegistryBuilder::AddCreatorsFromSoHandles(loaded_handles, registry),
                    "[CUSTOM OP] Build model custom op registry failed.");
  GE_CHK_STATUS_RET(LoadCustomOps(om_load_helper, registry), "[CUSTOM OP] Load custom ops to registry failed.");
  GE_CHK_STATUS_RET(ValidateCustomOpsDeserialized(ge_root_model, registry),
                    "[CUSTOM OP] Validate model custom ops deserialized failed.");
  ge_root_model->SetCustomOpRegistry(registry);
  return SUCCESS;
}

Status ModelHelper::LoadOpSoBin(const OmFileLoadHelper &om_load_helper, const GeRootModelPtr &ge_root_model,
                                std::vector<CustomOpSoHandlePtr> &loaded_handles) const {
  ModelPartition partition_kernel_def;
  if (om_load_helper.GetModelPartition(ModelPartitionType::SO_BINS, partition_kernel_def, 0U) != SUCCESS) {
    return SUCCESS;
  }
  GELOGD("Kernels partition size:%" PRIu64 "", partition_kernel_def.size);
  if (ge_root_model->LoadSoBinData(partition_kernel_def.data, partition_kernel_def.size)) {
    auto root_graph = ge_root_model->GetRootGraph();
    GE_ASSERT_NOTNULL(root_graph);
    std::map<std::string, ge::OpSoBinPtr> bin_file_buffer;
    auto all_so_bin = ge_root_model->GetAllSoBin();
    std::vector<OpSoBinPtr> custom_op_so_bins;
    for (const auto &op_so_bin_ptr : all_so_bin) {
      if (op_so_bin_ptr == nullptr) {
        continue;
      }
      if (op_so_bin_ptr->GetSoBinType() == SoBinType::kAutofuse) {
        if (op_so_bin_ptr->GetSoName() == "guard_check.so") {
          std::string guard_data(reinterpret_cast<const char_t *>(op_so_bin_ptr->GetBinData()),
                                 op_so_bin_ptr->GetBinDataSize());
          (void)AttrUtils::SetStr(root_graph, "_guard_check_so_data", guard_data);
          GELOGD("Restored guard check so data to graph attr, size=%zu.", guard_data.size());
        } else {
          std::string so_path = op_so_bin_ptr->GetVendorName() + "/" + op_so_bin_ptr->GetSoName();
          bin_file_buffer[so_path] = op_so_bin_ptr;
          GELOGD("Added autofuse so_path:%s", so_path.c_str());
        }
      } else if (op_so_bin_ptr->GetSoBinType() == SoBinType::kCustomOp) {
        (void)custom_op_so_bins.emplace_back(op_so_bin_ptr);
      }
    }
    if (!bin_file_buffer.empty()) {
      (void)root_graph->SetExtAttr<std::map<std::string, ge::OpSoBinPtr>>("bin_file_buffer", bin_file_buffer);
    }
    GE_ASSERT_SUCCESS(LoadCustomOpSoBins(custom_op_so_bins, loaded_handles));
    SaveOpSoInfo(ge_root_model);
    GELOGD("Load so bin store success");
  } else {
    GELOGW("Load so bin store unsuccessful");
    GE_ASSERT_TRUE(partition_kernel_def.size == 0U,
                   "Load so bin store failed when SO_BINS partition is non-empty, size:%" PRIu64,
                   partition_kernel_def.size);
  }
  return SUCCESS;
}

Status ModelHelper::LoadCustomOpSoBins(const std::vector<OpSoBinPtr> &custom_so_bins,
                                       std::vector<CustomOpSoHandlePtr> &loaded_handles) const {
  if (custom_so_bins.empty()) {
    return SUCCESS;
  }
  GE_ASSERT_SUCCESS(CustomOpSoLoader::GetInstance().LoadCustomOpSoBins(custom_so_bins, loaded_handles),
                    "Load custom op so bins from SO_BINS failed.");
  return SUCCESS;
}

void ModelHelper::SaveOpSoInfo(const GeRootModelPtr &ge_root_model) const {
  SoInOmInfo so_info;
  (void)ge::AttrUtils::GetStr(*(model_.get()), "host_env_os", so_info.os_info);
  (void)ge::AttrUtils::GetStr(*(model_.get()), "host_env_cpu", so_info.cpu_info);
  (void)ge::AttrUtils::GetStr(*(model_.get()), ATTR_MODEL_OPP_VERSION, so_info.opp_version);
  (void)ge::AttrUtils::GetStr(*(model_.get()), ATTR_MODEL_COMPILER_VERSION, so_info.compiler_version);
  GELOGD("Save so info with host_env_os:%s, host_env_cpu:%s, opp_version:%s, compiler_version:%s",
         so_info.os_info.c_str(), so_info.cpu_info.c_str(), so_info.opp_version.c_str(),
         so_info.compiler_version.c_str());
  ge_root_model->SetSoInOmInfo(so_info);
}
}  // namespace ge
