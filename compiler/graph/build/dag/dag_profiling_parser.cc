/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/build/dag/dag_profiling_parser.h"
#include "graph/build/dag/dag_log.h"
#include <fstream>
#include <sstream>

namespace minidag {

int32_t ProfilingParser::FindColumnIndex(const std::vector<std::string> &headers,
                                          const std::string &column_name) {
  for (size_t i = 0; i < headers.size(); ++i) {
    if (headers[i] == column_name) {
      return static_cast<int32_t>(i);
    }
  }
  return -1;
}

void ProfilingParser::CalculateCoreCounts(const std::string &task_type,
                                           uint32_t block_num,
                                           uint32_t mix_block_num,
                                           size_t &cube_block_num,
                                           size_t &vec_block_num) {
  cube_block_num = 0;
  vec_block_num = 0;

  if (task_type == "AI_CORE") {
    cube_block_num = block_num;
  } else if (task_type == "AI_VECTOR_CORE") {
    vec_block_num = block_num;
  } else if (task_type == "MIX_AIC") {
    cube_block_num = block_num;
    vec_block_num = mix_block_num;
  } else if (task_type == "MIX_AIV") {
    cube_block_num = mix_block_num;
    vec_block_num = block_num;
  }
}

graphStatus ProfilingParser::ParseRow(const std::vector<std::string> &fields,
                                       const std::vector<int32_t> &col_indices,
                                       ProfilingData &data) {
  const int32_t kOpNameIdx = 0;
  const int32_t kTaskTypeIdx = 1;
  const int32_t kDurationIdx = 2;
  const int32_t kBlockNumIdx = 3;
  const int32_t kMixBlockNumIdx = 4;

  for (size_t i = 0; i < col_indices.size(); ++i) {
    if (col_indices[i] < 0 || static_cast<size_t>(col_indices[i]) >= fields.size()) {
      MINIDAG_LOG_WARN("Invalid column index %d for field size %zu", col_indices[i], fields.size());
      return graphStatus::FAILED;
    }
  }

  data.op_name = fields[col_indices[kOpNameIdx]];

  try {
    data.execution_time = std::stof(fields[col_indices[kDurationIdx]]);
  } catch (const std::exception &e) {
    MINIDAG_LOG_WARN("Failed to parse Task Duration: %s", fields[col_indices[kDurationIdx]].c_str());
    return graphStatus::FAILED;
  }

  uint32_t block_num = 0;
  uint32_t mix_block_num = 0;
  try {
    block_num = static_cast<uint32_t>(std::stoul(fields[col_indices[kBlockNumIdx]]));
    mix_block_num = static_cast<uint32_t>(std::stoul(fields[col_indices[kMixBlockNumIdx]]));
  } catch (const std::exception &e) {
    MINIDAG_LOG_WARN("Failed to parse Block Num or Mix Block Num");
    return graphStatus::FAILED;
  }

  const std::string &task_type = fields[col_indices[kTaskTypeIdx]];
  CalculateCoreCounts(task_type, block_num, mix_block_num, data.cube_block_num, data.vec_block_num);

  return graphStatus::SUCCESS;
}

static const std::vector<std::string> kRequiredColumns = {
  "Op Name",
  "Task Type",
  "Task Duration(us)",
  "Block Num",
  "Mix Block Num"
};

graphStatus ProfilingParser::Parse(const std::string &csv_path,
                                    std::unordered_map<std::string, ProfilingData> &profiles) {
  profiles.clear();

  std::ifstream file(csv_path);
  if (!file.is_open()) {
    MINIDAG_LOG_WARN("Failed to open profiling file: %s", csv_path.c_str());
    return graphStatus::FAILED;
  }

  std::string header_line;
  if (!std::getline(file, header_line)) {
    MINIDAG_LOG_WARN("Failed to read header line from: %s", csv_path.c_str());
    return graphStatus::FAILED;
  }

  std::vector<std::string> headers;
  std::stringstream header_ss(header_line);
  std::string header_field;
  while (std::getline(header_ss, header_field, ',')) {
    headers.push_back(header_field);
  }

  std::vector<int32_t> col_indices;
  for (const auto &col_name : kRequiredColumns) {
    int32_t idx = FindColumnIndex(headers, col_name);
    if (idx < 0) {
      MINIDAG_LOG_WARN("Required column not found: %s", col_name.c_str());
      return graphStatus::FAILED;
    }
    col_indices.push_back(idx);
  }

  std::string data_line;
  while (std::getline(file, data_line)) {
    if (data_line.empty()) {
      continue;
    }

    std::vector<std::string> fields;
    std::stringstream data_ss(data_line);
    std::string field;
    while (std::getline(data_ss, field, ',')) {
      fields.push_back(field);
    }

    ProfilingData data;
    if (ParseRow(fields, col_indices, data) == graphStatus::SUCCESS) {
      profiles[data.op_name] = data;
    }
  }

  file.close();
  MINIDAG_LOG_INFO("Parsed %zu profiling records from %s", profiles.size(), csv_path.c_str());
  return graphStatus::SUCCESS;
}

}  // namespace minidag
