/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "history/history_registry_writer.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "history/history_registry_utils.h"
#include "ge_running_env/path_utils.h"
#include "graph/operator_factory.h"
#include "op_desc_utils.h"

using namespace ge;
using namespace ge::es::history;
namespace {
OpDescPtr CreateOpDesc(const std::string &op_type) {
  auto op = ge::OperatorFactory::CreateOperator(("op_test_" + op_type).c_str(), op_type.c_str());
  return ge::OpDescUtils::GetOpDescFromOperator(op);
}

template <typename F>
void ExpectInvalidArgumentErrorContains(F &&fn, const std::string &expected_substr) {
  try {
    fn();
    FAIL() << "Expected std::invalid_argument";
  } catch (const std::invalid_argument &e) {
    EXPECT_NE(std::string(e.what()).find(expected_substr), std::string::npos);
  } catch (...) {
    FAIL() << "Expected std::invalid_argument";
  }
}

void ExpectOpJsonPhony1i1o(const nlohmann::json &op_1i_1o_json) {
  ASSERT_TRUE(op_1i_1o_json.is_object());
  ASSERT_TRUE(op_1i_1o_json.contains("op_type"));
  ASSERT_TRUE(op_1i_1o_json.contains("inputs"));
  ASSERT_TRUE(op_1i_1o_json.contains("outputs"));
  ASSERT_TRUE(op_1i_1o_json.contains("attrs"));
  ASSERT_TRUE(op_1i_1o_json.contains("subgraphs"));

  ASSERT_TRUE(op_1i_1o_json["inputs"].is_array());
  ASSERT_EQ(op_1i_1o_json["inputs"].size(), 1U);
  EXPECT_EQ(op_1i_1o_json["inputs"][0]["name"], "x");
  EXPECT_EQ(op_1i_1o_json["inputs"][0]["type"], "INPUT");
  EXPECT_EQ(op_1i_1o_json["inputs"][0]["dtype"], "TensorType::ALL()");

  ASSERT_TRUE(op_1i_1o_json["outputs"].is_array());
  ASSERT_EQ(op_1i_1o_json["outputs"].size(), 1U);
  EXPECT_EQ(op_1i_1o_json["outputs"][0]["name"], "y");
  EXPECT_EQ(op_1i_1o_json["outputs"][0]["type"], "OUTPUT");
  EXPECT_EQ(op_1i_1o_json["outputs"][0]["dtype"], "TensorType::ALL()");

  ASSERT_TRUE(op_1i_1o_json["attrs"].is_array());
  ASSERT_EQ(op_1i_1o_json["attrs"].size(), 1U);
  EXPECT_EQ(op_1i_1o_json["attrs"][0]["name"], "index");
  EXPECT_EQ(op_1i_1o_json["attrs"][0]["type"], "Int");
  EXPECT_EQ(op_1i_1o_json["attrs"][0]["required"], false);
  EXPECT_EQ(op_1i_1o_json["attrs"][0]["default_value"], "0");

  ASSERT_TRUE(op_1i_1o_json["subgraphs"].is_array());
  EXPECT_TRUE(op_1i_1o_json["subgraphs"].empty());
}

void ExpectOpJsonPhonyIf(const nlohmann::json &op_if_json) {
  ASSERT_NE(op_if_json, nullptr);
  ASSERT_TRUE(op_if_json.contains("op_type"));
  ASSERT_TRUE(op_if_json.contains("inputs"));
  ASSERT_TRUE(op_if_json.contains("outputs"));
  ASSERT_TRUE(op_if_json.contains("attrs"));
  ASSERT_TRUE(op_if_json.contains("subgraphs"));

  ASSERT_TRUE(op_if_json["inputs"].is_array());
  ASSERT_EQ(op_if_json["inputs"].size(), 2U);
  EXPECT_EQ(op_if_json["inputs"][0]["name"], "cond");
  EXPECT_EQ(op_if_json["inputs"][0]["type"], "INPUT");
  EXPECT_EQ(op_if_json["inputs"][0]["dtype"], "TensorType::ALL()");
  EXPECT_EQ(op_if_json["inputs"][1]["name"], "input");
  EXPECT_EQ(op_if_json["inputs"][1]["type"], "DYNAMIC_INPUT");
  EXPECT_EQ(op_if_json["inputs"][1]["dtype"], "TensorType::ALL()");

  ASSERT_TRUE(op_if_json["outputs"].is_array());
  ASSERT_EQ(op_if_json["outputs"].size(), 1U);
  EXPECT_EQ(op_if_json["outputs"][0]["name"], "output");
  EXPECT_EQ(op_if_json["outputs"][0]["type"], "DYNAMIC_OUTPUT");
  EXPECT_EQ(op_if_json["outputs"][0]["dtype"], "TensorType::ALL()");

  ASSERT_TRUE(op_if_json["attrs"].is_array());
  EXPECT_TRUE(op_if_json["attrs"].empty());

  ASSERT_TRUE(op_if_json["subgraphs"].is_array());
  ASSERT_EQ(op_if_json["subgraphs"].size(), 2U);
  EXPECT_EQ(op_if_json["subgraphs"][0]["name"], "then_branch");
  EXPECT_EQ(op_if_json["subgraphs"][0]["type"], "STATIC");
  EXPECT_EQ(op_if_json["subgraphs"][1]["name"], "else_branch");
  EXPECT_EQ(op_if_json["subgraphs"][1]["type"], "STATIC");
}
}  // namespace

class HistoryRegistryWriterUT : public ::testing::Test {
 protected:
  void SetUp() override {
    output_dir_ = std::string("./history_registry_writer_test_") + std::to_string(getpid()) + "/";
  }

  void TearDown() override {
    system(("rm -rf " + output_dir_).c_str());
  }

  std::string output_dir_;
};

TEST_F(HistoryRegistryWriterUT, WriteRegistryGeneratedFiles) {
  auto op_desc1 = CreateOpDesc("phony_1i_1o");
  auto op_desc2 = CreateOpDesc("phony_If");
  ASSERT_NE(op_desc1, nullptr);
  ASSERT_NE(op_desc2, nullptr);
  std::vector<OpDescPtr> ops{op_desc1, op_desc2};

  HistoryRegistryWriter::WriteRegistry(output_dir_, "8.0.RC1", "2024-09-30", "master", ops);

  const std::string index_path = output_dir_ + "index.json";
  const std::string metadata_path = output_dir_ + "registry/8.0.RC1/metadata.json";
  const std::string operators_path = output_dir_ + "registry/8.0.RC1/operators.json";

  EXPECT_TRUE(IsFile(index_path.c_str()));
  EXPECT_TRUE(IsFile(metadata_path.c_str()));
  EXPECT_TRUE(IsFile(operators_path.c_str()));

  nlohmann::json index_json;
  std::string error_msg;
  ASSERT_TRUE(ReadJsonFile(index_path, index_json, error_msg));
  ASSERT_TRUE(index_json.contains("version"));
  EXPECT_EQ(index_json["version"], "1.0.0");
  ASSERT_TRUE(index_json.contains("releases"));
  ASSERT_TRUE(index_json["releases"].is_array());
  ASSERT_EQ(index_json["releases"].size(), 1U);
  EXPECT_EQ(index_json["releases"][0]["release_version"], "8.0.RC1");
  EXPECT_EQ(index_json["releases"][0]["release_date"], "2024-09-30");

  nlohmann::json meta_json;
  ASSERT_TRUE(ReadJsonFile(metadata_path, meta_json, error_msg));
  EXPECT_EQ(meta_json["release_version"], "8.0.RC1");
  EXPECT_EQ(meta_json["branch_name"], "master");

  nlohmann::json ops_json;
  ASSERT_TRUE(ReadJsonFile(operators_path, ops_json, error_msg));
  ASSERT_TRUE(ops_json.contains("operators"));
  ASSERT_TRUE(ops_json["operators"].is_array());
  ASSERT_EQ(ops_json["operators"].size(), 2U);
  ExpectOpJsonPhony1i1o(ops_json["operators"][0]);
  ExpectOpJsonPhonyIf(ops_json["operators"][1]);
}

TEST_F(HistoryRegistryWriterUT, WriteRegistryUpdatesIndex) {
  auto op_desc = CreateOpDesc("phony_1i_1o");
  ASSERT_NE(op_desc, nullptr);
  std::vector<OpDescPtr> ops{op_desc};

  HistoryRegistryWriter::WriteRegistry(output_dir_, "8.0.RC1", "2024-09-30", "", ops);
  const std::string index_path = output_dir_ + "index.json";
  nlohmann::json index_json;
  std::string error_msg;
  ASSERT_TRUE(ReadJsonFile(index_path, index_json, error_msg));
  ASSERT_TRUE(index_json.contains("releases"));
  ASSERT_TRUE(index_json["releases"].is_array());
  ASSERT_EQ(index_json["releases"].size(), 1U);
  EXPECT_EQ(index_json["releases"][0]["release_version"], "8.0.RC1");

  const std::string metadata_path_8_0_RC1 = output_dir_ + "registry/8.0.RC1/metadata.json";
  const std::string operators_path_8_0_RC1 = output_dir_ + "registry/8.0.RC1/operators.json";
  EXPECT_TRUE(IsFile(metadata_path_8_0_RC1.c_str()));
  EXPECT_TRUE(IsFile(operators_path_8_0_RC1.c_str()));

  const std::string metadata_path_8_0_RC2 = output_dir_ + "registry/8.0.RC2/metadata.json";
  const std::string operators_path_8_0_RC2 = output_dir_ + "registry/8.0.RC2/operators.json";
  EXPECT_FALSE(IsFile(metadata_path_8_0_RC2.c_str()));
  EXPECT_FALSE(IsFile(operators_path_8_0_RC2.c_str()));

  // update
  HistoryRegistryWriter::WriteRegistry(output_dir_, "8.0.RC2", "2024-10-15", "", ops);
  ASSERT_TRUE(ReadJsonFile(index_path, index_json, error_msg));
  ASSERT_TRUE(index_json.contains("releases"));
  ASSERT_TRUE(index_json["releases"].is_array());
  ASSERT_EQ(index_json["releases"].size(), 2U);
  EXPECT_EQ(index_json["releases"][0]["release_version"], "8.0.RC1");
  EXPECT_EQ(index_json["releases"][1]["release_version"], "8.0.RC2");

  EXPECT_TRUE(IsFile(metadata_path_8_0_RC2.c_str()));
  EXPECT_TRUE(IsFile(operators_path_8_0_RC2.c_str()));
}

TEST_F(HistoryRegistryWriterUT, WriteRegistryThrowsOnEmptyReleaseVersion) {
  auto op_desc = CreateOpDesc("phony_1i_1o");
  ASSERT_NE(op_desc, nullptr);
  std::vector<OpDescPtr> ops{op_desc};

  ExpectInvalidArgumentErrorContains(
      [&]() { HistoryRegistryWriter::WriteRegistry(output_dir_, "", "2024-09-30", "master", ops); },
      "The required parameter release_version for history registry generator is not set.");
}

TEST_F(HistoryRegistryWriterUT, WriteRegistryThrowsOnDuplicateReleaseVersion) {
  auto op_desc = CreateOpDesc("phony_1i_1o");
  ASSERT_NE(op_desc, nullptr);
  std::vector<OpDescPtr> ops{op_desc};

  HistoryRegistryWriter::WriteRegistry(output_dir_, "8.0.RC1", "2024-09-30", "", ops);
  ExpectInvalidArgumentErrorContains(
      [&]() { HistoryRegistryWriter::WriteRegistry(output_dir_, "8.0.RC1", "2024-10-01", "", ops); },
      "Given release_version already exists in index, please check index.json or use another version: ");
}

TEST_F(HistoryRegistryWriterUT, WriteRegistryThrowsOnInvalidReleaseDate) {
  auto op_desc = CreateOpDesc("phony_1i_1o");
  ASSERT_NE(op_desc, nullptr);
  std::vector<OpDescPtr> ops{op_desc};

  ExpectInvalidArgumentErrorContains(
      [&]() { HistoryRegistryWriter::WriteRegistry(output_dir_, "8.0.RC1", "not-a-date", "master", ops); },
      "Given release_date parameter for history registry generator is not in the correct format (YYYY-MM-DD).");
}

TEST_F(HistoryRegistryWriterUT, WriteRegistryEmptyReleaseDateUsesCurrentDate) {
  auto op_desc = CreateOpDesc("phony_1i_1o");
  ASSERT_NE(op_desc, nullptr);
  std::vector<OpDescPtr> ops{op_desc};

  // release_date 为空时使用当前日期，不应抛异常
  EXPECT_NO_THROW(HistoryRegistryWriter::WriteRegistry(output_dir_, "8.0.RC1", "", "master", ops));

  nlohmann::json index_json;
  std::string error_msg;
  ASSERT_TRUE(ReadJsonFile(output_dir_ + "index.json", index_json, error_msg));
  ASSERT_EQ(index_json["releases"].size(), 1U);
  // 验证 release_date 是合法的 YYYY-MM-DD 格式
  std::string date = index_json["releases"][0]["release_date"].get<std::string>();
  EXPECT_TRUE(ValidateReleaseDateFormat(date));
}

TEST_F(HistoryRegistryWriterUT, WriteRegistrySkipsInvalidExistingIndexEntry) {
  // 手动构造一个含非法 entry 的 index.json
  system(("mkdir -p " + output_dir_).c_str());
  std::ofstream(output_dir_ + "index.json") << R"({
    "version": "1.0.0",
    "releases": [
      {"release_version": "7.0.RC1", "release_date": "2024-06-01"},
      {"release_version": "7.1.RC1", "release_date": "20240701"},
      "not_an_object"
    ]
  })";

  auto op_desc = CreateOpDesc("phony_1i_1o");
  ASSERT_NE(op_desc, nullptr);
  std::vector<OpDescPtr> ops{op_desc};

  // 写入新版本时，非法 entry 应被跳过，不影响正常写入
  EXPECT_NO_THROW(HistoryRegistryWriter::WriteRegistry(output_dir_, "8.0.RC1", "2024-09-30", "", ops));

  nlohmann::json index_json;
  std::string error_msg;
  ASSERT_TRUE(ReadJsonFile(output_dir_ + "index.json", index_json, error_msg));
  ASSERT_EQ(index_json["releases"].size(), 2U);
  EXPECT_EQ(index_json["releases"][0]["release_version"], "7.0.RC1");
  EXPECT_EQ(index_json["releases"][1]["release_version"], "8.0.RC1");
}

TEST_F(HistoryRegistryWriterUT, ValidateReleaseDateFormat_Valid) {
  EXPECT_TRUE(ValidateReleaseDateFormat("2024-01-15"));
  EXPECT_TRUE(ValidateReleaseDateFormat("1900-01-01"));
  EXPECT_TRUE(ValidateReleaseDateFormat("9999-12-31"));
}

TEST_F(HistoryRegistryWriterUT, ValidateReleaseDateFormat_Invalid) {
  EXPECT_FALSE(ValidateReleaseDateFormat("2024-13-01"));
  EXPECT_FALSE(ValidateReleaseDateFormat("2024-00-01"));
  EXPECT_FALSE(ValidateReleaseDateFormat("2024-01-32"));
  EXPECT_FALSE(ValidateReleaseDateFormat("2024-01-00"));
  EXPECT_FALSE(ValidateReleaseDateFormat("1899-01-01"));
  EXPECT_FALSE(ValidateReleaseDateFormat("10000-01-01"));
  EXPECT_FALSE(ValidateReleaseDateFormat("invalid"));
  EXPECT_FALSE(ValidateReleaseDateFormat(""));
  EXPECT_FALSE(ValidateReleaseDateFormat("2024/01/15"));
  EXPECT_FALSE(ValidateReleaseDateFormat("2024-1-1"));
}

TEST_F(HistoryRegistryWriterUT, ReadJsonFile_NonExistentFile) {
  nlohmann::json out;
  std::string error_msg;
  EXPECT_FALSE(ReadJsonFile("/tmp/nonexistent_file_12345.json", out, error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST_F(HistoryRegistryWriterUT, ReadJsonFile_InvalidJsonContent) {
  std::string tmp_path = output_dir_ + "invalid.json";
  std::ofstream ofs(tmp_path);
  ofs << "{invalid json content}";
  ofs.close();
  nlohmann::json out;
  std::string error_msg;
  EXPECT_FALSE(ReadJsonFile(tmp_path, out, error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST_F(HistoryRegistryWriterUT, ReadJsonFile_EmptyFile) {
  std::string tmp_path = output_dir_ + "empty.json";
  std::ofstream ofs(tmp_path);
  ofs.close();
  nlohmann::json out;
  std::string error_msg;
  EXPECT_FALSE(ReadJsonFile(tmp_path, out, error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST_F(HistoryRegistryWriterUT, ValidateRequireString_Missing) {
  nlohmann::json j = nlohmann::json::object();
  std::string error_msg;
  EXPECT_FALSE(ValidateRequireString(j, "key", error_msg, ""));
  EXPECT_FALSE(error_msg.empty());
}

TEST_F(HistoryRegistryWriterUT, ValidateRequireString_NotString) {
  nlohmann::json j = {{"key", 123}};
  std::string error_msg;
  EXPECT_FALSE(ValidateRequireString(j, "key", error_msg, ""));
  EXPECT_FALSE(error_msg.empty());
}

TEST_F(HistoryRegistryWriterUT, ValidateRequireString_Valid) {
  nlohmann::json j = {{"key", "value"}};
  std::string error_msg;
  EXPECT_TRUE(ValidateRequireString(j, "key", error_msg, ""));
}

TEST_F(HistoryRegistryWriterUT, ValidateRequireArray_Missing) {
  nlohmann::json j = nlohmann::json::object();
  std::string error_msg;
  EXPECT_FALSE(ValidateRequireArray(j, "key", error_msg, ""));
  EXPECT_FALSE(error_msg.empty());
}

TEST_F(HistoryRegistryWriterUT, ValidateRequireArray_NotArray) {
  nlohmann::json j = {{"key", "not_array"}};
  std::string error_msg;
  EXPECT_FALSE(ValidateRequireArray(j, "key", error_msg, ""));
  EXPECT_FALSE(error_msg.empty());
}

TEST_F(HistoryRegistryWriterUT, ValidateRequireArray_Valid) {
  nlohmann::json j = {{"key", nlohmann::json::array()}};
  std::string error_msg;
  EXPECT_TRUE(ValidateRequireArray(j, "key", error_msg, ""));
}

TEST_F(HistoryRegistryWriterUT, ValidateOptionalString_Valid) {
  nlohmann::json j = nlohmann::json::object();
  std::string error_msg;
  EXPECT_TRUE(ValidateOptionalString(j, "key", error_msg, ""));
  j["key"] = "value";
  EXPECT_TRUE(ValidateOptionalString(j, "key", error_msg, ""));
}

TEST_F(HistoryRegistryWriterUT, ValidateOptionalString_NotString) {
  nlohmann::json j = {{"key", 123}};
  std::string error_msg;
  EXPECT_FALSE(ValidateOptionalString(j, "key", error_msg, ""));
  EXPECT_FALSE(error_msg.empty());
}

TEST_F(HistoryRegistryWriterUT, ValidateIndexJson_NotObject) {
  nlohmann::json j = nlohmann::json::array();
  std::string error_msg;
  EXPECT_FALSE(ValidateIndexJson(j, error_msg));
}

TEST_F(HistoryRegistryWriterUT, ValidateIndexJson_MissingVersion) {
  nlohmann::json j = {{"releases", nlohmann::json::array()}};
  std::string error_msg;
  EXPECT_FALSE(ValidateIndexJson(j, error_msg));
}

TEST_F(HistoryRegistryWriterUT, ValidateIndexJson_MissingReleases) {
  nlohmann::json j = {{"version", "1.0.0"}};
  std::string error_msg;
  EXPECT_FALSE(ValidateIndexJson(j, error_msg));
}

TEST_F(HistoryRegistryWriterUT, ValidateIndexJson_Valid) {
  nlohmann::json j = {{"version", "1.0.0"}, {"releases", nlohmann::json::array()}};
  std::string error_msg;
  EXPECT_TRUE(ValidateIndexJson(j, error_msg));
}

TEST_F(HistoryRegistryWriterUT, ValidateIndexReleaseEntryJson_NotObject) {
  nlohmann::json entry = nlohmann::json::array();
  std::set<std::string> seen;
  std::string error_msg;
  EXPECT_FALSE(ValidateIndexReleaseEntryJson(entry, 0, seen, error_msg));
}

TEST_F(HistoryRegistryWriterUT, ValidateIndexReleaseEntryJson_DuplicateVersion) {
  nlohmann::json entry = {{"release_version", "1.0"}, {"release_date", "2024-01-01"}};
  std::set<std::string> seen = {"1.0"};
  std::string error_msg;
  EXPECT_FALSE(ValidateIndexReleaseEntryJson(entry, 0, seen, error_msg));
}

TEST_F(HistoryRegistryWriterUT, ValidateIndexReleaseEntryJson_InvalidDate) {
  nlohmann::json entry = {{"release_version", "1.0"}, {"release_date", "invalid"}};
  std::set<std::string> seen;
  std::string error_msg;
  EXPECT_FALSE(ValidateIndexReleaseEntryJson(entry, 0, seen, error_msg));
}

TEST_F(HistoryRegistryWriterUT, ValidateIndexReleaseEntryJson_Valid) {
  nlohmann::json entry = {{"release_version", "1.0"}, {"release_date", "2024-01-01"}};
  std::set<std::string> seen;
  std::string error_msg;
  EXPECT_TRUE(ValidateIndexReleaseEntryJson(entry, 0, seen, error_msg));
}

TEST_F(HistoryRegistryWriterUT, GetCurrentDate_ReturnsNonEmpty) {
  std::string date;
  EXPECT_TRUE(GetCurrentDate(date));
  EXPECT_FALSE(date.empty());
  EXPECT_EQ(date.size(), 10U);
}

TEST_F(HistoryRegistryWriterUT, WriteRegistry_WithBranchName) {
  std::vector<OpDescPtr> ops;
  EXPECT_NO_THROW(HistoryRegistryWriter::WriteRegistry(output_dir_, "9.0.RC1", "2024-03-01", "main_branch", ops));
  nlohmann::json meta;
  std::string error_msg;
  ASSERT_TRUE(ReadJsonFile(output_dir_ + "registry/9.0.RC1/metadata.json", meta, error_msg));
  EXPECT_EQ(meta["branch_name"], "main_branch");
}

TEST_F(HistoryRegistryWriterUT, WriteRegistry_EmptyBranchName) {
  std::vector<OpDescPtr> ops;
  EXPECT_NO_THROW(HistoryRegistryWriter::WriteRegistry(output_dir_, "10.0.RC1", "2024-04-01", "", ops));
  nlohmann::json meta;
  std::string error_msg;
  ASSERT_TRUE(ReadJsonFile(output_dir_ + "registry/10.0.RC1/metadata.json", meta, error_msg));
  EXPECT_FALSE(meta.contains("branch_name"));
}
