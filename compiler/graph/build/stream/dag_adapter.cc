/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/build/stream/dag_adapter.h"
#include "framework/common/debug/ge_log.h"
#include "common/checker.h"
#include "graph/utils/node_adapter.h"
#include "graph/debug/ge_attr_define.h"
#include "common/ge_common/ge_types.h"
#include "graph/build/stream/stream_utils.h"
#include "graph/ge_local_context.h"
#include "graph/utils/op_type_utils.h"
#include "external/ge_common/ge_common_api_types.h"
#include "platform/platform_info.h"
#include "graph/build/dag/dag_profiling_parser.h"
#include <cstdlib>
#include <cstring>

namespace ge {

namespace {
constexpr size_t kCsvSuffixLength = 4U;

std::string GetOpNameWithoutScope(const std::string &full_name) {
  size_t pos = full_name.find_last_of('/');
  if (pos != std::string::npos && pos + 1 < full_name.size()) {
    return full_name.substr(pos + 1);
  }
  return full_name;
}
}

graphStatus DAGAdapter::ToGEStatus(minidag::graphStatus status) {
  switch (status) {
    case minidag::graphStatus::SUCCESS:
      return GRAPH_SUCCESS;
    case minidag::graphStatus::FAILED:
      return GRAPH_FAILED;
    case minidag::graphStatus::INVALID_NODE:
    case minidag::graphStatus::INVALID_EDGE:
      return GRAPH_FAILED;
    case minidag::graphStatus::NODE_NOT_FOUND:
      return GE_GRAPH_GRAPH_NODE_NULL;
    default:
      return GRAPH_FAILED;
  }
}

graphStatus DAGAdapter::FromGEGraph(const ConstGraphPtr &ge_graph,
                                    std::shared_ptr<minidag::DAGGraph> &dag,
                                    bool &has_profiled_node_cost) {
  GE_ASSERT_NOTNULL(ge_graph, "FromGEGraph failed: ge_graph is null");
  has_profiled_node_cost = false;

  // 1. 获取图名称
  AscendString name;
  GE_ASSERT_SUCCESS(ge_graph->GetName(name), "FromGEGraph failed: GetName returned error");
  GELOGI("FromGEGraph start: graph name=%s", name.GetString());
  dag = std::make_shared<minidag::DAGGraph>(name.GetString());
  GE_ASSERT_NOTNULL(dag, "FromGEGraph failed: create DAGGraph failed");

  // 2. 转换节点（并设置topo_id）
  auto nodes_ret = ConvertNodes(ge_graph, *dag, has_profiled_node_cost);
  GE_ASSERT_SUCCESS(nodes_ret, "FromGEGraph failed: ConvertNodes returned %d",
                    static_cast<int>(nodes_ret));

  // 3. 转换边（数据边和控制边）
  auto edges_ret = ConvertEdges(ge_graph, *dag);
  GE_ASSERT_SUCCESS(edges_ret, "FromGEGraph failed: ConvertEdges returned %d",
                    static_cast<int>(edges_ret));

  // 获取设备资源信息
  GE_CHK_STATUS_RET(FillDeviceResource(*dag), "Failed to fill device resource info.");

  GELOGI("FromGEGraph done: nodes=%zu, edges=%zu",
          dag->GetNodeCount(), dag->GetEdgeCount());
  return GRAPH_SUCCESS;
}

graphStatus DAGAdapter::ConvertNodes(const ConstGraphPtr &ge_graph,
                                     minidag::DAGGraph &dag,
                                     bool &has_profiled_node_cost) {
  GE_ASSERT_NOTNULL(ge_graph);
  has_profiled_node_cost = false;
  std::string profiling_path = ResolveProfilingPath();
  std::unordered_map<std::string, minidag::ProfilingData> profiles;
  LoadProfilingData(profiling_path, profiles);

  auto gnodes = ge_graph->GetDirectNode();
  int64_t topo_id = 0;

  for (const auto& gnode : gnodes) {
    AscendString name, type;
    GE_ASSERT_SUCCESS(gnode.GetName(name), "ConvertNodes failed: GetName failed for gnode");
    GE_ASSERT_SUCCESS(gnode.GetType(type), "ConvertNodes failed: GetType failed for gnode %s", name.GetString());

    auto dag_node = dag.AddNode(name.GetString(), type.GetString());
    GE_ASSERT_NOTNULL(dag_node, "ConvertNodes failed: duplicate node name %s, type %s",
                      name.GetString(), type.GetString());

    auto node = NodeAdapter::GNode2Node(gnode);
    GE_ASSERT_NOTNULL(node);
    auto op_desc = node->GetOpDesc();
    GE_ASSERT_NOTNULL(op_desc);

    std::string node_name(name.GetString());
    bool profiled_cost_matched = false;
    minidag::NodeCost cost = BuildNodeCost(node_name, node, op_desc, profiles, profiled_cost_matched);
    has_profiled_node_cost = has_profiled_node_cost || profiled_cost_matched;
    // 设置串行标记
    std::string stream_label;
    (void) AttrUtils::GetStr(op_desc, ATTR_NAME_STREAM_LABEL, stream_label);
    if (!stream_label.empty()) {
      dag_node->SetSerialFlag(stream_label);
    }
    // stream id如果是-1的节点一定不是device有计算任务的节点，所以执行耗时设置为0
    if (op_desc->GetStreamId() == INVALID_STREAM_ID) {
      cost.execution_time = 0.0f;
    }
    dag_node->SetCost(cost);
    dag_node->SetTopoId(topo_id++);
  }

  GELOGI("ConvertNodes done: total=%zu", gnodes.size());
  return GRAPH_SUCCESS;
}

std::string DAGAdapter::ResolveProfilingPath() {
  // Environment variable for explicitly specifying the op_summary.csv path. MiniDAG switches to the
  // weighted algorithm in the later stream merge stage only when at least one node in the current
  // graph matches profiling data from this file; otherwise it keeps the original non-weighted strategy.
  const char *env_minidag_path = std::getenv("MINIDAG_PROFILING_PATH");
  if (env_minidag_path == nullptr || strlen(env_minidag_path) == 0) {
    return "";
  }
  std::string env_str(env_minidag_path);
  if (env_str.size() >= kCsvSuffixLength && env_str.substr(env_str.size() - kCsvSuffixLength) == ".csv") {
    std::string profiling_path = env_str;
    GELOGI("Profiling path overridden by MINIDAG_PROFILING_PATH (csv file): %s", profiling_path.c_str());
    return profiling_path;
  }
  GELOGW("MINIDAG_PROFILING_PATH only accepts csv file path, but got: %s", env_str.c_str());
  return "";
}

void DAGAdapter::LoadProfilingData(
    const std::string &profiling_path,
    std::unordered_map<std::string, minidag::ProfilingData> &profiles) {
  if (profiling_path.empty()) {
    return;
  }
  auto parse_ret = minidag::ProfilingParser::Parse(profiling_path, profiles);
  if (parse_ret != minidag::graphStatus::SUCCESS) {
    GELOGW("Parse profiling data failed from %s, using default cost values.", profiling_path.c_str());
    profiles.clear();
    return;
  }
  GELOGI("Parsed %zu profiling records from %s", profiles.size(), profiling_path.c_str());
}

minidag::NodeCost DAGAdapter::BuildNodeCost(
    const std::string &node_name,
    const NodePtr &node,
    const OpDescPtr &op_desc,
    const std::unordered_map<std::string, minidag::ProfilingData> &profiles,
    bool &profiled_cost_matched) {
  minidag::NodeCost cost;
  profiled_cost_matched = false;
  auto it = profiles.find(node_name);
  if (it == profiles.end()) {
    std::string name_without_scope = GetOpNameWithoutScope(node_name);
    it = profiles.find(name_without_scope);
  }

  if (it != profiles.end()) {
    cost.execution_time = it->second.execution_time;
    cost.cube_block_num = it->second.cube_block_num;
    cost.vec_block_num = it->second.vec_block_num;
    profiled_cost_matched = true;
    GELOGI("Use profiling cost for %s: time=%f, cube=%zu, vec=%zu",
           node_name.c_str(), cost.execution_time, cost.cube_block_num, cost.vec_block_num);
    return cost;
  }

  int32_t block_dim = -1;
  (void)AttrUtils::GetInt(op_desc, TVM_ATTR_NAME_BLOCKDIM, block_dim);
  if (block_dim == -1) {
    return cost;
  }
  if (StreamUtils::IsAicNode(node)) {
    cost.cube_block_num = block_dim;
  }
  if (StreamUtils::IsAivNode(node)) {
    cost.vec_block_num = block_dim;
  }
  return cost;
}

graphStatus DAGAdapter::ConvertEdges(const ConstGraphPtr &ge_graph, minidag::DAGGraph &dag) {
  int64_t data_edge_count = 0;
  int64_t control_edge_count = 0;

  for (const auto &gnode : ge_graph->GetDirectNode()) {
    AscendString src_name;
    GE_ASSERT_SUCCESS(gnode.GetName(src_name), "ConvertEdges failed: GetName failed for src gnode");
    auto src_node = dag.FindNode(src_name.GetString());
    GE_ASSERT_NOTNULL(src_node, "ConvertEdges failed: src_node not found in dag: %s", src_name.GetString());
    GE_ASSERT_SUCCESS(ConvertDataEdgesForNode(gnode, src_node, dag, data_edge_count));
    GE_ASSERT_SUCCESS(ConvertControlEdgesForNode(gnode, src_node, dag, control_edge_count));
  }

  GELOGI("ConvertEdges done: data_edges=%ld, control_edges=%ld",
          data_edge_count, control_edge_count);
  return GRAPH_SUCCESS;
}

graphStatus DAGAdapter::ConvertDataEdgesForNode(
    const GNode &gnode,
    const std::shared_ptr<minidag::DAGNode> &src_node,
    minidag::DAGGraph &dag,
    int64_t &edge_count) {
  for (size_t i = 0; i < gnode.GetOutputsSize(); ++i) {
    auto dst_pairs = gnode.GetOutDataNodesAndPortIndexs(static_cast<int32_t>(i));
    for (const auto& [dst_gnode, dst_port] : dst_pairs) {
      if (dst_gnode == nullptr) {
        continue;
      }
      AscendString dst_name;
      GE_ASSERT_GRAPH_SUCCESS(dst_gnode->GetName(dst_name),
                              "ConvertDataEdgesForNode failed: GetName failed for dst gnode");
      auto dst_node = dag.FindNode(dst_name.GetString());
      GE_ASSERT_NOTNULL(dst_node, "ConvertDataEdgesForNode failed: dst_node not found: %s", dst_name.GetString());

      minidag::graphStatus ret = dag.AddEdge(src_node, static_cast<int32_t>(i), dst_node, dst_port);
      if (ret != minidag::graphStatus::SUCCESS) {
        GELOGE(GRAPH_FAILED, "ConvertDataEdgesForNode failed: AddEdge failed");
        return ToGEStatus(ret);
      }
      ++edge_count;
    }
  }
  return GRAPH_SUCCESS;
}

graphStatus DAGAdapter::ConvertControlEdgesForNode(
    const GNode &gnode,
    const std::shared_ptr<minidag::DAGNode> &src_node,
    minidag::DAGGraph &dag,
    int64_t &edge_count) {
  for (const auto &dst_gnode : gnode.GetOutControlNodes()) {
    if (dst_gnode == nullptr) {
      continue;
    }
    AscendString dst_name;
    GE_ASSERT_SUCCESS(dst_gnode->GetName(dst_name),
                       "ConvertControlEdgesForNode failed: GetName failed for dst gnode");
    auto dst_node = dag.FindNode(dst_name.GetString());
    GE_ASSERT_NOTNULL(dst_node, "ConvertControlEdgesForNode failed: dst_node not found: %s", dst_name.GetString());

    minidag::graphStatus ret = dag.AddEdge(src_node, -1, dst_node, -1);
    if (ret != minidag::graphStatus::SUCCESS) {
      GELOGE(GRAPH_FAILED, "ConvertControlEdgesForNode failed: AddEdge failed");
      return ToGEStatus(ret);
    }
    ++edge_count;
  }
  return GRAPH_SUCCESS;
}

graphStatus DAGAdapter::RefreshStreamIdsToGE(
    const minidag::DAGGraph &dag,
    const ConstGraphPtr &ge_graph,
    StreamPassContext &context) {
  GE_ASSERT_NOTNULL(ge_graph, "RefreshStreamIdsToGE failed: ge_graph is null");

  int64_t success_count = 0;
  int64_t skip_count = 0;
  int64_t filtered_count = 0;

  for (const auto &dag_node : dag.GetAllNodes()) {
    // 通过节点名称查找GE GNode
    AscendString node_name(dag_node->GetName().c_str());
    auto gnode = ge_graph->FindNodeByName(node_name);
    if (gnode == nullptr) {
      GELOGD("Node not found in GE graph: %s", dag_node->GetName().c_str());
      ++skip_count;
      continue;
    }

    const auto compute_node = NodeAdapter::GNode2Node(*gnode);
    auto op_desc = compute_node->GetOpDesc();
    GE_ASSERT_NOTNULL(op_desc);
    // Adapter 层过滤：跳过 NetOutput/Label相关的节点
    const auto &node_type = op_desc->GetTypePtr();
    bool rts_label_node = false;
    (void) AttrUtils::GetBool(op_desc, ATTR_NAME_RTS_LABEL_NODE, rts_label_node);
    if ((node_type == "NetOutput") || rts_label_node) {
      GELOGD("Skip special node: %s", dag_node->GetName().c_str());
      ++filtered_count;
      continue;
    }

    int64_t stream_id = dag_node->GetStreamId();
    if (stream_id == INVALID_STREAM_ID) {
      GELOGD("Node %s has invalid stream_id, skip", dag_node->GetName().c_str());
      ++skip_count;
      continue;
    }

    // 设置到GE
    const auto &origin_stream_id = op_desc->GetStreamId();
    if (origin_stream_id == INVALID_STREAM_ID) {
      GELOGD("Node %s origin stream id is invalid, skip", dag_node->GetName().c_str());
      ++skip_count;
      continue;
    }
    auto ret = context.SetStreamId(*gnode, stream_id);
    if (ret != GRAPH_SUCCESS) {
      GE_LOGE("SetStreamId failed for node %s, stream_id=%ld, ret=%d",
              dag_node->GetName().c_str(), stream_id, ret);
      return ret;
    }
    AscendString name;
    GE_ASSERT_GRAPH_SUCCESS(gnode->GetName(name));
    GELOGI("node %s refresh stream id from %ld to %ld", name.GetString(), origin_stream_id, stream_id);
    ++success_count;
  }

  GELOGI("RefreshStreamIdsToGE done: success=%ld, skip=%ld, filtered=%ld",
          success_count, skip_count, filtered_count);
  return GRAPH_SUCCESS;
}

Status DAGAdapter::FillDeviceResource(minidag::DAGGraph &dag) {
  minidag::DeviceResourceInfo resource;

  std::string soc_version;
  auto ret = GetThreadLocalContext().GetOption(ge::SOC_VERSION, soc_version);
  if (ret != GRAPH_SUCCESS || soc_version.empty()) {
    GELOGW("Failed to get soc_version from thread local context, DeviceResourceInfo will use default values -1.");
    return SUCCESS;
  }

  fe::PlatformInfo platform_info;
  fe::OptionalInfo optional_info;
  if (fe::PlatformInfoManager::GeInstance().InitializePlatformInfo() != 0U ||
      fe::PlatformInfoManager::GeInstance().GetPlatformInfo(soc_version, platform_info, optional_info) != 0U) {
    GELOGW("Failed to get platform info for soc_version %s, DeviceResourceInfo will use default values -1.",
           soc_version.c_str());
    return SUCCESS;
  }
  resource.cube_core_num = platform_info.soc_info.ai_core_cnt;
  resource.vector_core_num = platform_info.soc_info.vector_core_cnt;

  GELOGI("DeviceResourceInfo: soc_version=%s, aicore=%ld, vector=%ld",
         soc_version.c_str(), resource.cube_core_num, resource.vector_core_num);

  dag.SetDeviceResource(resource);
  return SUCCESS;
}
}  // namespace ge
