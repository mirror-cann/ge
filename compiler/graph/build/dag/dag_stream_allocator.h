/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_DAG_MINIDAG_DAG_STREAM_ALLOCATOR_H_
#define GE_GRAPH_BUILD_DAG_MINIDAG_DAG_STREAM_ALLOCATOR_H_

#include <cstdint>
#include <memory>

#include "graph/build/dag/dag_stream_merger.h"
#include "graph/build/dag/dag_graph.h"

namespace minidag {
struct StreamAllocConfig {
    int64_t max_stream_id = -1;      // 入参：-1 表示无上限，>=0 表示上限
    int64_t required_streams = 0;    // 出参：策略实际使用的 stream 数量
    int64_t base_stream_id = 0;      // 入参：基准流ID
    StreamMergeStrategy merge_strategy = StreamMergeStrategy::kLoadBalance;  // 策略选择，默认LoadBalance
};

class DagStreamAllocator {
 public:
  static void ByPathCover(DAGGraph& graph, StreamAllocConfig& config);
  DagStreamAllocator() = delete;
};
}  // namespace minidag
#endif  // GE_GRAPH_BUILD_DAG_MINIDAG_DAG_STREAM_ALLOCATOR_H_