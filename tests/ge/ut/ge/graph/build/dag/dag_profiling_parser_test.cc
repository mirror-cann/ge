/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <fstream>
#include <cstdio>
#include "graph/build/dag/dag_profiling_parser.h"

namespace minidag {
namespace test {

class ProfilingParserTest : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(ProfilingParserTest, FindColumnIndex_ColumnExists_ReturnsCorrectIndex) {
  std::vector<std::string> headers = {"Op Name", "Task Type", "Task Duration(us)", "Block Num", "Mix Block Num"};

  EXPECT_EQ(ProfilingParser::FindColumnIndex(headers, "Op Name"), 0);
  EXPECT_EQ(ProfilingParser::FindColumnIndex(headers, "Task Type"), 1);
  EXPECT_EQ(ProfilingParser::FindColumnIndex(headers, "Task Duration(us)"), 2);
  EXPECT_EQ(ProfilingParser::FindColumnIndex(headers, "Block Num"), 3);
  EXPECT_EQ(ProfilingParser::FindColumnIndex(headers, "Mix Block Num"), 4);
}

TEST_F(ProfilingParserTest, FindColumnIndex_ColumnNotExists_ReturnsNegativeOne) {
  std::vector<std::string> headers = {"Op Name", "Task Type"};

  EXPECT_EQ(ProfilingParser::FindColumnIndex(headers, "Nonexistent Column"), -1);
}

TEST_F(ProfilingParserTest, FindColumnIndex_EmptyHeaders_ReturnsNegativeOne) {
  std::vector<std::string> headers;

  EXPECT_EQ(ProfilingParser::FindColumnIndex(headers, "Op Name"), -1);
}

TEST_F(ProfilingParserTest, FindColumnIndex_ColumnOrderChanged_ReturnsCorrectIndex) {
  std::vector<std::string> headers = {"Task Type", "Op Name", "Block Num", "Task Duration(us)", "Mix Block Num"};

  EXPECT_EQ(ProfilingParser::FindColumnIndex(headers, "Op Name"), 1);
  EXPECT_EQ(ProfilingParser::FindColumnIndex(headers, "Task Duration(us)"), 3);
}

TEST_F(ProfilingParserTest, CalculateCoreCounts_AiCoreType) {
  size_t cube_block_num = 0;
  size_t vec_block_num = 0;

  ProfilingParser::CalculateCoreCounts("AI_CORE", 10, 0, cube_block_num, vec_block_num);

  EXPECT_EQ(cube_block_num, 10);
  EXPECT_EQ(vec_block_num, 0);
}

TEST_F(ProfilingParserTest, CalculateCoreCounts_AiVectorCoreType) {
  size_t cube_block_num = 0;
  size_t vec_block_num = 0;

  ProfilingParser::CalculateCoreCounts("AI_VECTOR_CORE", 8, 0, cube_block_num, vec_block_num);

  EXPECT_EQ(cube_block_num, 0);
  EXPECT_EQ(vec_block_num, 8);
}

TEST_F(ProfilingParserTest, CalculateCoreCounts_MixAicType) {
  size_t cube_block_num = 0;
  size_t vec_block_num = 0;

  ProfilingParser::CalculateCoreCounts("MIX_AIC", 10, 5, cube_block_num, vec_block_num);

  EXPECT_EQ(cube_block_num, 10);
  EXPECT_EQ(vec_block_num, 5);
}

TEST_F(ProfilingParserTest, CalculateCoreCounts_MixAivType) {
  size_t cube_block_num = 0;
  size_t vec_block_num = 0;

  ProfilingParser::CalculateCoreCounts("MIX_AIV", 10, 5, cube_block_num, vec_block_num);

  EXPECT_EQ(cube_block_num, 5);
  EXPECT_EQ(vec_block_num, 10);
}

TEST_F(ProfilingParserTest, CalculateCoreCounts_UnknownType) {
  size_t cube_block_num = 0;
  size_t vec_block_num = 0;

  ProfilingParser::CalculateCoreCounts("AI_CPU", 10, 0, cube_block_num, vec_block_num);

  EXPECT_EQ(cube_block_num, 0);
  EXPECT_EQ(vec_block_num, 0);
}

TEST_F(ProfilingParserTest, CalculateCoreCounts_EmptyType) {
  size_t cube_block_num = 0;
  size_t vec_block_num = 0;

  ProfilingParser::CalculateCoreCounts("", 10, 0, cube_block_num, vec_block_num);

  EXPECT_EQ(cube_block_num, 0);
  EXPECT_EQ(vec_block_num, 0);
}

TEST_F(ProfilingParserTest, CalculateCoreCounts_ZeroBlockNum) {
  size_t cube_block_num = 0;
  size_t vec_block_num = 0;

  ProfilingParser::CalculateCoreCounts("AI_CORE", 0, 0, cube_block_num, vec_block_num);

  EXPECT_EQ(cube_block_num, 0);
  EXPECT_EQ(vec_block_num, 0);
}

TEST_F(ProfilingParserTest, ParseRow_NormalData) {
  std::vector<std::string> fields = {"conv1", "AI_CORE", "100.5", "8", "0"};

  std::vector<int32_t> col_indices = {0, 1, 2, 3, 4};

  ProfilingData data;
  graphStatus ret = ProfilingParser::ParseRow(fields, col_indices, data);

  EXPECT_EQ(ret, graphStatus::SUCCESS);
  EXPECT_EQ(data.op_name, "conv1");
  EXPECT_EQ(data.execution_time, 100.5f);
  EXPECT_EQ(data.cube_block_num, 8);
  EXPECT_EQ(data.vec_block_num, 0);
}

TEST_F(ProfilingParserTest, ParseRow_AiVectorCore) {
  std::vector<std::string> fields = {"vec_op", "AI_VECTOR_CORE", "50.0", "4", "0"};

  std::vector<int32_t> col_indices = {0, 1, 2, 3, 4};

  ProfilingData data;
  graphStatus ret = ProfilingParser::ParseRow(fields, col_indices, data);

  EXPECT_EQ(ret, graphStatus::SUCCESS);
  EXPECT_EQ(data.vec_block_num, 4);
  EXPECT_EQ(data.cube_block_num, 0);
}

TEST_F(ProfilingParserTest, ParseRow_MixAicType) {
  std::vector<std::string> fields = {"mix_aic_op", "MIX_AIC", "200.0", "10", "5"};

  std::vector<int32_t> col_indices = {0, 1, 2, 3, 4};

  ProfilingData data;
  graphStatus ret = ProfilingParser::ParseRow(fields, col_indices, data);

  EXPECT_EQ(ret, graphStatus::SUCCESS);
  EXPECT_EQ(data.cube_block_num, 10);
  EXPECT_EQ(data.vec_block_num, 5);
}

TEST_F(ProfilingParserTest, ParseRow_MixAivType) {
  std::vector<std::string> fields = {"mix_aiv_op", "MIX_AIV", "150.0", "8", "3"};

  std::vector<int32_t> col_indices = {0, 1, 2, 3, 4};

  ProfilingData data;
  graphStatus ret = ProfilingParser::ParseRow(fields, col_indices, data);

  EXPECT_EQ(ret, graphStatus::SUCCESS);
  EXPECT_EQ(data.cube_block_num, 3);
  EXPECT_EQ(data.vec_block_num, 8);
}

TEST_F(ProfilingParserTest, ParseRow_InvalidColumnIndex_ReturnsFailed) {
  std::vector<std::string> fields = {"conv1", "AI_CORE", "100.5", "8", "0"};

  // Negative column index
  std::vector<int32_t> col_indices = {-1, 1, 2, 3, 4};

  ProfilingData data;
  graphStatus ret = ProfilingParser::ParseRow(fields, col_indices, data);

  EXPECT_NE(ret, graphStatus::SUCCESS);
}

TEST_F(ProfilingParserTest, ParseRow_OutOfBoundsIndex_ReturnsFailed) {
  std::vector<std::string> fields = {"conv1", "AI_CORE", "100.5", "8", "0"};

  // Index >= fields.size()
  std::vector<int32_t> col_indices = {0, 1, 2, 3, 10};

  ProfilingData data;
  graphStatus ret = ProfilingParser::ParseRow(fields, col_indices, data);

  EXPECT_NE(ret, graphStatus::SUCCESS);
}

TEST_F(ProfilingParserTest, ParseRow_InvalidDuration_ReturnsFailed) {
  std::vector<std::string> fields = {"conv1", "AI_CORE", "not_a_number", "8", "0"};
  std::vector<int32_t> col_indices = {0, 1, 2, 3, 4};

  ProfilingData data;
  graphStatus ret = ProfilingParser::ParseRow(fields, col_indices, data);

  EXPECT_NE(ret, graphStatus::SUCCESS);
}

TEST_F(ProfilingParserTest, ParseRow_InvalidBlockNum_ReturnsFailed) {
  std::vector<std::string> fields = {"conv1", "AI_CORE", "100.5", "invalid", "0"};
  std::vector<int32_t> col_indices = {0, 1, 2, 3, 4};

  ProfilingData data;
  graphStatus ret = ProfilingParser::ParseRow(fields, col_indices, data);

  EXPECT_NE(ret, graphStatus::SUCCESS);
}

class ProfilingParserFileTest : public testing::Test {
 protected:
  void SetUp() override {
    test_csv_path_ = "/tmp/test_op_summary.csv";
  }

  void TearDown() override {
    if (!test_csv_path_.empty()) {
      std::remove(test_csv_path_.c_str());
    }
  }

  void CreateTestCsv(const std::string &content) {
    std::ofstream file(test_csv_path_);
    file << content;
    file.close();
  }

  std::string test_csv_path_;
};

TEST_F(ProfilingParserFileTest, Parse_NormalCsv_ReturnsProfiles) {
  CreateTestCsv(
      "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
      "conv1,AI_CORE,100.5,8,0\n"
      "vec_op,AI_VECTOR_CORE,50.0,4,0\n"
      "mix_aic_op,MIX_AIC,200.0,10,5\n"
      "mix_aiv_op,MIX_AIV,150.0,8,3\n");

  std::unordered_map<std::string, ProfilingData> profiles;
  graphStatus ret = ProfilingParser::Parse(test_csv_path_, profiles);

  EXPECT_EQ(ret, graphStatus::SUCCESS);
  EXPECT_EQ(profiles.size(), 4);

  EXPECT_EQ(profiles["conv1"].execution_time, 100.5f);
  EXPECT_EQ(profiles["conv1"].cube_block_num, 8);

  EXPECT_EQ(profiles["vec_op"].vec_block_num, 4);

  EXPECT_EQ(profiles["mix_aic_op"].cube_block_num, 10);
  EXPECT_EQ(profiles["mix_aic_op"].vec_block_num, 5);

  EXPECT_EQ(profiles["mix_aiv_op"].cube_block_num, 3);
  EXPECT_EQ(profiles["mix_aiv_op"].vec_block_num, 8);
}

TEST_F(ProfilingParserFileTest, Parse_MissingRequiredColumn_ReturnsFailed) {
  CreateTestCsv(
      "Op Name,Task Type,Block Num\n"
      "conv1,AI_CORE,8\n");

  std::unordered_map<std::string, ProfilingData> profiles;
  graphStatus ret = ProfilingParser::Parse(test_csv_path_, profiles);

  EXPECT_NE(ret, graphStatus::SUCCESS);
  EXPECT_EQ(profiles.size(), 0);
}

TEST_F(ProfilingParserFileTest, Parse_ColumnOrderChanged_ReturnsProfiles) {
  CreateTestCsv(
      "Task Type,Op Name,Block Num,Task Duration(us),Mix Block Num\n"
      "AI_CORE,conv1,8,100.5,0\n");

  std::unordered_map<std::string, ProfilingData> profiles;
  graphStatus ret = ProfilingParser::Parse(test_csv_path_, profiles);

  EXPECT_EQ(ret, graphStatus::SUCCESS);
  EXPECT_EQ(profiles["conv1"].execution_time, 100.5f);
  EXPECT_EQ(profiles["conv1"].cube_block_num, 8);
}

TEST_F(ProfilingParserFileTest, Parse_FileNotExist_ReturnsFailed) {
  std::unordered_map<std::string, ProfilingData> profiles;
  graphStatus ret = ProfilingParser::Parse("/tmp/nonexistent.csv", profiles);

  EXPECT_NE(ret, graphStatus::SUCCESS);
}

TEST_F(ProfilingParserFileTest, Parse_EmptyFile_ReturnsFailed) {
  CreateTestCsv("");

  std::unordered_map<std::string, ProfilingData> profiles;
  graphStatus ret = ProfilingParser::Parse(test_csv_path_, profiles);

  EXPECT_NE(ret, graphStatus::SUCCESS);
  EXPECT_TRUE(profiles.empty());
}

TEST_F(ProfilingParserFileTest, Parse_EmptyLineAndInvalidRow_SkipsInvalidEntries) {
  CreateTestCsv(
      "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
      "\n"
      "conv1,AI_CORE,100.5,8,0\n"
      "bad_row,AI_CORE,invalid,8,0\n");

  std::unordered_map<std::string, ProfilingData> profiles;
  graphStatus ret = ProfilingParser::Parse(test_csv_path_, profiles);

  EXPECT_EQ(ret, graphStatus::SUCCESS);
  ASSERT_EQ(profiles.size(), 1U);
  EXPECT_EQ(profiles["conv1"].execution_time, 100.5f);
  EXPECT_EQ(profiles["conv1"].cube_block_num, 8U);
}

}  // namespace test
}  // namespace minidag
