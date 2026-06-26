/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <set>

#include "graph/build/stream/dag_stream_allocator_pass.h"
#include "graph/build/stream/dag_adapter.h"
#include "graph/build/dag/dag_stream_allocator.h"
#include "register/register_custom_pass.h"
#include "graph/ge_context.h"
#include "framework/common/debug/ge_log.h"
#include "graph/utils/node_utils.h"
#include "graph/utils/node_adapter.h"
#include "graph/utils/graph_utils_ex.h"
#include "common/checker.h"

namespace ge {
namespace {
constexpr int32_t kMaxStreamLimit = 64;
constexpr int32_t kDefaultMaxPhysicalStreams = 8;

const char_t *GetStrategyName(const minidag::StreamMergeStrategy strategy) {
  switch (strategy) {
    case minidag::StreamMergeStrategy::kLoadBalance:
      return "LoadBalance";
    case minidag::StreamMergeStrategy::kMainStream:
      return "MainStream";
    case minidag::StreamMergeStrategy::kWeightedLoadBalance:
      return "WeightedLoadBalance";
    default:
      return "Unknown";
  }
}

bool ParseStreamConfig(const std::string &multi_stream_mode, int64_t &out_max_stream_id,
                       minidag::StreamMergeStrategy &out_strategy) {
  auto readable = GetContext().GetReadableName("ge.autoMultistreamParallelMode");

  auto colon_pos = multi_stream_mode.find(':');
  if (colon_pos == std::string::npos) {
    (void)REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char_t *>({"value", "parameter", "reason"}),
                                    std::vector<const char_t *>({multi_stream_mode.c_str(), readable.c_str(),
                                                                 "Format error: missing colon separator."}));
    GELOGE(FAILED, "%s format error: missing colon separator, value=%s.", readable.c_str(), multi_stream_mode.c_str());
    return false;
  }

  std::string algo = multi_stream_mode.substr(0, colon_pos);
  if (algo.empty()) {
    (void)REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char_t *>({"value", "parameter", "reason"}),
                                    std::vector<const char_t *>({multi_stream_mode.c_str(), readable.c_str(),
                                                                 "Format error: algo name is empty."}));
    GELOGE(FAILED, "%s format error: algo name is empty.", readable.c_str());
    return false;
  }

  if (algo == "MainStream") {
    out_strategy = minidag::StreamMergeStrategy::kMainStream;
  } else if (algo == "LoadBalance") {
    out_strategy = minidag::StreamMergeStrategy::kLoadBalance;
  } else {
    const auto invalid_strategy = static_cast<minidag::StreamMergeStrategy>(-1);
    const auto *strategy_name = GetStrategyName(invalid_strategy);
    const std::string reason = "Unknown merge strategy: algo=" + algo + ", strategy=" + strategy_name +
                               " (expected LoadBalance or MainStream).";
    (void)REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char_t *>({"value", "parameter", "reason"}),
                                    std::vector<const char_t *>({algo.c_str(), readable.c_str(), reason.c_str()}));
    GELOGE(FAILED, "Unknown merge strategy in %s: algo=%s, strategy=%s (expected LoadBalance or MainStream).",
           readable.c_str(), algo.c_str(), strategy_name);
    return false;
  }

  std::string max_str = multi_stream_mode.substr(colon_pos + 1);
  int32_t max_val_int32 = 0;
  std::string range_msg = "Invalid max_stream value, must be in range [1, " + std::to_string(kMaxStreamLimit) + "].";
  if (ge::ConvertToInt32(max_str, max_val_int32) != SUCCESS || max_val_int32 <= 0 || max_val_int32 > kMaxStreamLimit) {
    (void)REPORT_PREDEFINED_ERR_MSG(
        "E10001", std::vector<const char_t *>({"value", "parameter", "reason"}),
        std::vector<const char_t *>({max_str.c_str(), readable.c_str(), range_msg.c_str()}));
    GELOGE(FAILED, "Invalid max_stream value in %s: %s (must be in range [1, %d]).", readable.c_str(), max_str.c_str(),
           kMaxStreamLimit);
    return false;
  }

  out_max_stream_id = static_cast<int64_t>(max_val_int32) - 1;
  const auto *strategy_name = GetStrategyName(out_strategy);
  GELOGI("Parsed config: strategy=%s, max_stream_id=%ld.", strategy_name, out_max_stream_id);
  return true;
}

Status RunMiniDAGStreamPassForComputGraph(const ConstGraphPtr &graph, StreamPassContext &context,
                                          int64_t effective_max_stream_id, minidag::StreamMergeStrategy strategy) {
  GE_ASSERT_NOTNULL(graph);
  // 1. 获取base_stream_id
  const int64_t base_stream_id = context.AllocateNextStreamId();

  // 2. 构建DAG
  std::shared_ptr<minidag::DAGGraph> dag;
  bool has_profiled_node_cost = false;
  GE_ASSERT_GRAPH_SUCCESS(DAGAdapter::FromGEGraph(graph, dag, has_profiled_node_cost),
                          "MiniDAGStreamPass failed: FromGEGraph returned error");

  // 3. 执行ByPathCover
  minidag::StreamAllocConfig config{effective_max_stream_id, 0, base_stream_id};
  const auto final_strategy = has_profiled_node_cost ? minidag::StreamMergeStrategy::kWeightedLoadBalance : strategy;
  const auto *final_strategy_name = GetStrategyName(final_strategy);
  config.merge_strategy = final_strategy;
  if (has_profiled_node_cost) {
    const auto *input_strategy_name = GetStrategyName(strategy);
    GELOGI("MiniDAGStreamPass graph %s final strategy=%s, reason=matched profiling node cost (override from %s).",
           dag->GetName().c_str(), final_strategy_name, input_strategy_name);
  } else {
    GELOGD("MiniDAGStreamPass graph %s final strategy=%s, reason=no matched profiling node cost.",
           dag->GetName().c_str(), final_strategy_name);
  }
  minidag::DagStreamAllocator::ByPathCover(*dag, config);

  // 4. 分配物理流并更新context
  for (int64_t i = 0; i < config.required_streams; ++i) {
    (void)context.AllocateNextStreamId();
  }

  // 5. 写回GE图
  GE_ASSERT_GRAPH_SUCCESS(DAGAdapter::RefreshStreamIdsToGE(*dag, graph, context), "RefreshStreamIdsToGE failed");
  return SUCCESS;
}
}  // namespace

/**
 * MiniDAG Stream Pass 核心逻辑
 */
Status RunMiniDAGStreamPass(const ConstGraphPtr &graph, StreamPassContext &context) {
  // 1. 空图检查
  GE_ASSERT_NOTNULL(graph);

  // 2. 读取 ge.autoMultistreamParallelMode（主配置）
  std::string multi_stream_mode;
  GE_ASSERT_SUCCESS(GetContext().GetOption("ge.autoMultistreamParallelMode", multi_stream_mode),
                    "Failed to get ge.autoMultistreamParallelMode option");

  // 3. 特殊场景：LoadBalance 不带数字，使用默认 8 条流
  int64_t effective_max_stream_id = -1;
  minidag::StreamMergeStrategy strategy;
  if (multi_stream_mode == "LoadBalance") {
    strategy = minidag::StreamMergeStrategy::kLoadBalance;
    effective_max_stream_id = kDefaultMaxPhysicalStreams - 1;
    GELOGI("LoadBalance without stream count, using default 8 streams.");
  } else if (!ParseStreamConfig(multi_stream_mode, effective_max_stream_id, strategy)) {
    return FAILED;
  }

  GE_ASSERT_SUCCESS(RunMiniDAGStreamPassForComputGraph(graph, context, effective_max_stream_id, strategy),
                    "root graph RunMiniDAGStreamPass failed");
  for (const auto &sub_graph : graph->GetAllSubgraphs()) {
    GE_ASSERT_SUCCESS(RunMiniDAGStreamPassForComputGraph(sub_graph, context, effective_max_stream_id, strategy),
                      "sub graph RunMiniDAGStreamPass failed");
  }

  return SUCCESS;
}
}  // namespace ge

REGISTER_CUSTOM_PASS("MiniDAGStreamPass")
    .CustomAllocateStreamPassFn([](const ge::ConstGraphPtr &graph, ge::StreamPassContext &context) -> ge::Status {
      return ge::RunMiniDAGStreamPass(graph, context);
    })
    .Stage(ge::CustomPassStage::kAfterAssignLogicStream);
