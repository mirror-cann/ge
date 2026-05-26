/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <climits>
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

namespace ge {

namespace {
constexpr int32_t kMaxStreamLimit = 64;

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
        (void)REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char_t *>({"value", "parameter", "reason"}),
                                std::vector<const char_t *>({algo.c_str(), readable.c_str(),
                                                             "Unknown merge strategy, expected LoadBalance or MainStream."}));
        GELOGE(FAILED, "Unknown merge strategy in %s: algo=%s (expected LoadBalance or MainStream).",
               readable.c_str(), algo.c_str());
        return false;
    }

    std::string max_str = multi_stream_mode.substr(colon_pos + 1);
    int32_t max_val_int32 = 0;
    std::string range_msg = "Invalid max_stream value, must be in range [1, " + std::to_string(kMaxStreamLimit) + "].";
    if (ge::ConvertToInt32(max_str, max_val_int32) != SUCCESS || max_val_int32 <= 0 || max_val_int32 > kMaxStreamLimit) {
        (void)REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char_t *>({"value", "parameter", "reason"}),
                                std::vector<const char_t *>({max_str.c_str(), readable.c_str(), range_msg.c_str()}));
        GELOGE(FAILED, "Invalid max_stream value in %s: %s (must be in range [1, %d]).", readable.c_str(), max_str.c_str(), kMaxStreamLimit);
        return false;
    }

    out_max_stream_id = static_cast<int64_t>(max_val_int32) - 1;
    GELOGI("Parsed config: strategy=%s, max_stream_id=%ld.",
           (out_strategy == minidag::StreamMergeStrategy::kMainStream ? "MainStream" : "LoadBalance"),
           out_max_stream_id);
    return true;
}
}  // namespace

/**
 * MiniDAG Stream Pass 核心逻辑
 */
Status RunMiniDAGStreamPass(const ConstGraphPtr &graph, StreamPassContext &context) {
    // 1. 空图检查
    if (graph == nullptr) {
        GELOGE(FAILED, "RunMiniDAGStreamPass failed: graph is nullptr");
        return FAILED;
    }

    // 2. 读取 ge.autoMultistreamParallelMode（主配置）
    std::string multi_stream_mode;
    if (GetContext().GetOption("ge.autoMultistreamParallelMode", multi_stream_mode) != GRAPH_SUCCESS) {
        GELOGE(FAILED, "Failed to get ge.autoMultistreamParallelMode option");
        return FAILED;
    }

    // 3. 解析配置（解析失败直接返回 FAILED）
    int64_t effective_max_stream_id = -1;
    minidag::StreamMergeStrategy strategy;
    if (!ParseStreamConfig(multi_stream_mode, effective_max_stream_id, strategy)) {
        return FAILED;
    }

    // 4. 获取base_stream_id
    const int64_t base_stream_id = context.AllocateNextStreamId();

    // 5. 构建DAG
    std::shared_ptr<minidag::DAGGraph> dag;
    auto ret = minidag::DAGAdapter::FromGEGraph(graph, dag);
    if (ret != GRAPH_SUCCESS || dag == nullptr) {
        GELOGE(FAILED, "MiniDAGStreamPass failed: FromGEGraph returned error");
        return FAILED;
    }

    // 6. 执行ByPathCover
    minidag::StreamAllocConfig config{effective_max_stream_id, 0, base_stream_id};
    config.merge_strategy = strategy;
    minidag::DagStreamAllocator::ByPathCover(*dag, config);

    // 7. 分配物理流并更新context
    for (int64_t i = 0; i < config.required_streams; ++i) {
        (void)context.AllocateNextStreamId();
    }

    // 8. 写回GE图
    ret = minidag::DAGAdapter::RefreshStreamIdsToGE(*dag, graph, context);
    if (ret != GRAPH_SUCCESS) {
        GELOGE(FAILED, "RefreshStreamIdsToGE failed: ret=%d", ret);
        return FAILED;
    }

    return SUCCESS;
}

}  // namespace ge

REGISTER_CUSTOM_PASS("MiniDAGStreamPass")
    .CustomAllocateStreamPassFn([](const ge::ConstGraphPtr &graph,
                                    ge::StreamPassContext &context) -> ge::Status {
        return ge::RunMiniDAGStreamPass(graph, context);
    })
    .Stage(ge::CustomPassStage::kAfterAssignLogicStream);
