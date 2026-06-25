/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_DAG_DAG_STREAM_MERGER_H_
#define GE_GRAPH_BUILD_DAG_DAG_STREAM_MERGER_H_

#include <cstdint>
#include <vector>

#include "graph/build/dag/dag_graph.h"
#include "graph/build/dag/dag_types.h"

namespace minidag {

enum class StreamMergeStrategy {
  kLoadBalance = 0,
  kMainStream = 1,
  kWeightedLoadBalance = 2
};

struct StreamMergeOptions {
  StreamMergeStrategy strategy = StreamMergeStrategy::kMainStream;
  int32_t physical_stream_limit = 8;
  int32_t window_width = 6;
  int32_t candidate_limit = 6;
  int32_t low_conflict_limit = 3;
  int32_t light_stream_limit = 3;
  int32_t repair_moves = 0;
  double event_local_weight = 3.0;
  double window_balance_weight = 4.0;
  double total_load_weight = 1.0;
  double parallel_conflict_weight = 4.0;
  double new_flow_penalty_weight = 2.0;
  double main_stream_bonus_weight = 4.0;
  int32_t main_stream = 0;
};

class StreamMerger {
 public:
  explicit StreamMerger(const StreamMergeOptions &options = StreamMergeOptions());
  ~StreamMerger() = default;

  graphStatus Merge(const DAGGraph &dag,
                    const std::vector<std::vector<int32_t>> &logical_stream_routes,
                    std::vector<int32_t> &logical_to_physical_stream) const;

 private:
  StreamMergeOptions options_;
};

}  // namespace minidag

#endif  // GE_GRAPH_BUILD_DAG_DAG_STREAM_MERGER_H_