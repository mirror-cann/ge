/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_DAG_DAG_PROFILING_PARSER_H_
#define GE_GRAPH_BUILD_DAG_DAG_PROFILING_PARSER_H_

#include <string>
#include <unordered_map>
#include <vector>
#include "graph/build/dag/dag_types.h"

namespace minidag {

struct ProfilingData {
  std::string op_name;
  float execution_time = 0.0f;
  size_t cube_block_num = 0;
  size_t vec_block_num = 0;
};

class ProfilingParser {
 public:
  static graphStatus Parse(const std::string &csv_path,
                           std::unordered_map<std::string, ProfilingData> &profiles);
 private:
  static int32_t FindColumnIndex(const std::vector<std::string> &headers,
                                  const std::string &column_name);

  static void CalculateCoreCounts(const std::string &task_type,
                                   uint32_t block_num,
                                   uint32_t mix_block_num,
                                   size_t &cube_block_num,
                                   size_t &vec_block_num);

  static graphStatus ParseRow(const std::vector<std::string> &fields,
                               const std::vector<int32_t> &col_indices,
                               ProfilingData &data);
};

}  // namespace minidag

#endif  // GE_GRAPH_BUILD_DAG_DAG_PROFILING_PARSER_H_
