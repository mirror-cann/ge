/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "api/atc/cmd_flag_info.h"
#include "external/ge_common/ge_common_api_types.h"
#include "ge/ge_api_error_codes.h"
#include "common/ge_common/ge_inner_error_codes.h"
#include "ge_running_env/atc_utils.h"
#include "nlohmann/json.hpp"
#include "register/optimization_option_registry.h"

DECLARE_string(deterministic);
DECLARE_string(deterministic_level);
DECLARE_string(enable_attr_compression);

namespace ge {
std::set<std::string> &GetRawAppliedFlagNames();
std::map<std::string, std::string> &GetRawAppliedFlagOptions();
std::string StripCliOptionPrefix(const std::string &cli_name);
bool IsRawNonReplaceableCliOption(const std::string &flag_name);
std::map<std::string, std::string> BuildRawGeOptionToCliNameMap();
bool IsCliOptionExplicitlySet(const std::string &ge_option, const std::map<std::string, std::string> &option_to_cli_name,
                              const std::unordered_map<std::string, std::string> &user_options);
std::string GetRawCliName(const std::string &ge_option, const std::map<std::string, std::string> &option_to_cli_name);
Status ReadRawGeOptionsFile(const std::string &file_path, nlohmann::json &raw_json);
Status ParseRawCompileOptionLevel(const nlohmann::json &compile_options, const std::string &level,
                                  std::map<std::string, std::string> &raw_options,
                                  std::map<std::string, std::string> &raw_option_levels);
Status ParseRawCompileOptions(const nlohmann::json &raw_json, std::map<std::string, std::string> &raw_options,
                              std::map<std::string, std::string> &raw_option_levels);
Status CheckRawRegisteredOptimizationOptionValue(const std::string &option_name, const std::string &option_value);
Status ValidateRawGeOptionByCliParser(const std::string &option_name, const std::string &option_value,
                                      const std::string &cli_name);
Status ValidateRawGeOptionValue(const std::string &option_name, const std::string &option_value,
                                const std::string &cli_name);
Status ApplyRawGeOptionsToFlags(const std::map<std::string, std::string> &raw_options);
Status FilterAndValidateRawGeOptions(std::map<std::string, std::string> &raw_options,
                                     std::map<std::string, std::string> &raw_option_levels);
Status LoadRawGeOptions(std::map<std::string, std::string> &raw_options);
void MergeRawGeOptions(const std::map<std::string, std::string> &raw_options,
                       std::map<std::string, std::string> &options);

namespace {
constexpr const char *kRawUtInt32Flag = "raw_options_ut_int32";
constexpr const char *kRawUtNoCheckerOption = "ge.oo.raw_options_ut.no_checker";
constexpr const char *kRawUtNoCheckerFlag = "raw_options_ut_no_checker";
constexpr const char *kRawUtCheckerOption = "ge.oo.raw_options_ut.checker";
constexpr const char *kRawUtCheckerFlag = "raw_options_ut_checker";

bool RawOptionsUtChecker(const std::string &value, std::string &reason) {
  if (value == "ok") {
    return true;
  }
  reason = "expect ok";
  return false;
}

void EnsureRawOptionsUtFlagsRegistered() {
  static int32_t &int32_flag = flgs::RegisterParamInt32(kRawUtInt32Flag, 0, "raw options ut int32");
  static std::string &no_checker_flag = flgs::RegisterParamString(kRawUtNoCheckerFlag, "", "raw options ut string");
  static std::string &checker_flag = flgs::RegisterParamString(kRawUtCheckerFlag, "", "raw options ut checker");
  (void)int32_flag;
  (void)no_checker_flag;
  (void)checker_flag;
}

void RegisterRawOptionsUtOption(const std::string &option_name, const std::string &show_name,
                                OoInfo::ValueChecker checker) {
  OoInfo info(option_name, OoHierarchy::kH1);
  info.visibility = OoInfoUtils::GenOptVisibilityBits({OoEntryPoint::kAtc});
  info.levels = OoInfoUtils::GenOptLevelBits({OoLevel::kO3});
  info.show_infos[OoEntryPoint::kAtc] = {OoCategory::kGeneral, show_name};
  info.checker = checker;
  OptionRegistry::GetInstance().Register(info);
}

void EnsureRawOptionsUtOptimizationOptionsRegistered() {
  static const bool registered = []() {
    RegisterRawOptionsUtOption(kRawUtNoCheckerOption, kRawUtNoCheckerFlag, nullptr);
    RegisterRawOptionsUtOption(kRawUtCheckerOption, kRawUtCheckerFlag, RawOptionsUtChecker);
    return true;
  }();
  (void)registered;
}

class AtcRawOptionsUTest : public AtcTest {
 protected:
  void SetUp() override {
    EnsureRawOptionsUtFlagsRegistered();
    EnsureRawOptionsUtOptimizationOptionsRegistered();
    ResetRawState();
  }

  void TearDown() override {
    for (const auto &path : temp_files_) {
      (void)std::remove(path.c_str());
    }
    temp_files_.clear();
    ResetRawState();
    AtcTest::TearDown();
    FLAGS_deterministic = "0";
    FLAGS_deterministic_level = "0";
    FLAGS_enable_attr_compression = "true";
  }

  std::string WriteTempFile(const std::string &name, const std::string &content) {
    const std::string path = "/tmp/" + name + "_" + std::to_string(getpid()) + "_" +
                             std::to_string(temp_files_.size()) + ".json";
    std::ofstream file(path);
    file << content;
    temp_files_.push_back(path);
    return path;
  }

  void ResetRawState() {
    flgs::GetUserOptions().clear();
    GetRawAppliedFlagNames().clear();
    GetRawAppliedFlagOptions().clear();
    FLAGS_raw_ge_options = "";
    FLAGS_raw_ge_options_ignore_unsupported = false;
    FLAGS_log = "null";
    FLAGS_static_model_ops_lower_limit = "";
  }

 private:
  std::vector<std::string> temp_files_;
};
}  // namespace

TEST_F(AtcRawOptionsUTest, CmdFlagValueHelpersCheckAndSetRegisteredFlags) {
  EXPECT_EQ(flgs::CheckFlagValue(kRawUtInt32Flag, "abc"), flgs::GF_FAILED);
  EXPECT_EQ(flgs::CheckFlagValue(kRawUtInt32Flag, "123"), flgs::GF_SUCCESS);
  EXPECT_EQ(flgs::CheckFlagValue("raw_options_ut_unknown", "1"), flgs::GF_FAILED);

  EXPECT_EQ(flgs::SetFlagValue("display_model_info", "1"), flgs::GF_SUCCESS);
  EXPECT_EQ(FLAGS_display_model_info, "1");
  EXPECT_EQ(flgs::SetFlagValue("raw_options_ut_unknown", "1"), flgs::GF_FAILED);
}

TEST_F(AtcRawOptionsUTest, RawFileReaderRejectsMissingInvalidAndNonObjectJson) {
  nlohmann::json raw_json;
  EXPECT_EQ(ReadRawGeOptionsFile("/tmp/raw_options_ut_not_exist.json", raw_json), ge::FAILED);

  EXPECT_EQ(ReadRawGeOptionsFile(WriteTempFile("invalid_raw_options", "{ invalid json"), raw_json), ge::FAILED);
  EXPECT_EQ(ReadRawGeOptionsFile(WriteTempFile("array_raw_options", R"([{"compile options": {}}])"), raw_json),
            ge::FAILED);
  EXPECT_EQ(ReadRawGeOptionsFile(WriteTempFile("object_raw_options", R"({"compile options": {}})"), raw_json),
            ge::SUCCESS);
  EXPECT_TRUE(raw_json.is_object());
}

TEST_F(AtcRawOptionsUTest, RawCompileOptionsMergeByLevelPriorityAndRejectInvalidShapes) {
  std::map<std::string, std::string> raw_options;
  std::map<std::string, std::string> raw_option_levels;
  const nlohmann::json raw_json = {
      {"compile options",
       {{"global", {{"log", "info"}, {OPTION_STATIC_MODEL_OPS_LOWER_LIMIT, "1"}}},
        {"session", {{"log", "warning"}}},
        {"graph", {{"log", "error"}, {"deterministic", "1"}}}}}};

  EXPECT_EQ(ParseRawCompileOptions(raw_json, raw_options, raw_option_levels), ge::SUCCESS);
  EXPECT_EQ(raw_options["log"], "error");
  EXPECT_EQ(raw_options["deterministic"], "1");
  EXPECT_EQ(raw_options[OPTION_STATIC_MODEL_OPS_LOWER_LIMIT], "1");
  EXPECT_EQ(raw_option_levels["log"], "graph");

  EXPECT_EQ(ParseRawCompileOptions(nlohmann::json::object(), raw_options, raw_option_levels), ge::FAILED);
  EXPECT_EQ(ParseRawCompileOptions({{"compile options", {{"process", nlohmann::json::object()}}}}, raw_options,
                                   raw_option_levels),
            ge::FAILED);
  EXPECT_NE(ParseRawCompileOptions({{"compile options", {{"global", "bad"}}}}, raw_options, raw_option_levels),
            ge::SUCCESS);
  EXPECT_NE(ParseRawCompileOptions({{"compile options", {{"global", {{"", "1"}}}}}}, raw_options,
                                   raw_option_levels),
            ge::SUCCESS);
  EXPECT_NE(ParseRawCompileOptions({{"compile options", {{"global", {{"log", 1}}}}}}, raw_options,
                                   raw_option_levels),
            ge::SUCCESS);
  EXPECT_EQ(ParseRawCompileOptionLevel(nlohmann::json::object(), "graph", raw_options, raw_option_levels),
            ge::SUCCESS);
}

TEST_F(AtcRawOptionsUTest, RawOptionNameHelpersMapCliAndUserOptions) {
  const auto option_to_cli_name = BuildRawGeOptionToCliNameMap();
  EXPECT_EQ(StripCliOptionPrefix("--log"), "log");
  EXPECT_EQ(StripCliOptionPrefix("log"), "log");
  EXPECT_TRUE(IsRawNonReplaceableCliOption("soc_version"));
  EXPECT_FALSE(IsRawNonReplaceableCliOption("log"));
  EXPECT_EQ(GetRawCliName("not.supported", option_to_cli_name), "");
  EXPECT_EQ(GetRawCliName("log", option_to_cli_name), "--log");

  std::unordered_map<std::string, std::string> user_options;
  EXPECT_FALSE(IsCliOptionExplicitlySet("not.supported", option_to_cli_name, user_options));
  user_options["log"] = "warning";
  EXPECT_TRUE(IsCliOptionExplicitlySet("log", option_to_cli_name, user_options));
}

TEST_F(AtcRawOptionsUTest, RawOptionValueValidationUsesCliAndRegisteredCheckers) {
  EXPECT_EQ(ValidateRawGeOptionByCliParser("log", "info", "--log"), ge::SUCCESS);
  EXPECT_EQ(ValidateRawGeOptionByCliParser("log", "verbose", "--log"), ge::FAILED);

  EXPECT_EQ(ValidateRawGeOptionValue(OPTION_STATIC_MODEL_OPS_LOWER_LIMIT, "-1",
                                     "--static_model_ops_lower_limit"),
            ge::SUCCESS);
  EXPECT_NE(ValidateRawGeOptionValue(OPTION_STATIC_MODEL_OPS_LOWER_LIMIT, "bad",
                                     "--static_model_ops_lower_limit"),
            ge::SUCCESS);

  EXPECT_EQ(CheckRawRegisteredOptimizationOptionValue(kRawUtNoCheckerOption, "any"), ge::SUCCESS);
  EXPECT_EQ(CheckRawRegisteredOptimizationOptionValue(kRawUtCheckerOption, "ok"), ge::SUCCESS);
  EXPECT_EQ(CheckRawRegisteredOptimizationOptionValue(kRawUtCheckerOption, "bad"), ge::PARAM_INVALID);
  EXPECT_EQ(CheckRawRegisteredOptimizationOptionValue("ge.oo.raw_options_ut.not_registered", "bad"), ge::SUCCESS);
}

TEST_F(AtcRawOptionsUTest, FilterRawGeOptionsRejectsOrIgnoresUnsupportedAndValidatesSupported) {
  std::map<std::string, std::string> raw_options = {
      {OPTION_STATIC_MODEL_OPS_LOWER_LIMIT, "3"},
      {"log", "info"},
      {"not.supported", "value"},
  };
  std::map<std::string, std::string> raw_option_levels = {
      {OPTION_STATIC_MODEL_OPS_LOWER_LIMIT, "graph"},
      {"log", "session"},
      {"not.supported", "global"},
  };

  EXPECT_EQ(FilterAndValidateRawGeOptions(raw_options, raw_option_levels), ge::FAILED);

  raw_options = {{OPTION_STATIC_MODEL_OPS_LOWER_LIMIT, "3"}, {"log", "info"}, {"not.supported", "value"}};
  raw_option_levels = {{OPTION_STATIC_MODEL_OPS_LOWER_LIMIT, "graph"}, {"log", "session"},
                       {"not.supported", "global"}};
  FLAGS_raw_ge_options_ignore_unsupported = true;
  EXPECT_EQ(FilterAndValidateRawGeOptions(raw_options, raw_option_levels), ge::SUCCESS);
  EXPECT_EQ(raw_options.count("not.supported"), 0U);
  EXPECT_EQ(raw_option_levels.count("not.supported"), 0U);

  raw_options = {{"log", "verbose"}};
  raw_option_levels = {{"log", "graph"}};
  EXPECT_NE(FilterAndValidateRawGeOptions(raw_options, raw_option_levels), ge::SUCCESS);
}

TEST_F(AtcRawOptionsUTest, ApplyRawGeOptionsSetsFlagsAndRecordsAppliedRawFlags) {
  EXPECT_EQ(ApplyRawGeOptionsToFlags({{"not.supported", "value"}}), ge::SUCCESS);

  EXPECT_EQ(ApplyRawGeOptionsToFlags({{SOC_VERSION, "Ascend310"}}), ge::FAILED);
  EXPECT_NE(ApplyRawGeOptionsToFlags({{"log", "verbose"}}), ge::SUCCESS);

  ResetRawState();
  EXPECT_EQ(ApplyRawGeOptionsToFlags({{OPTION_STATIC_MODEL_OPS_LOWER_LIMIT, "5"}, {"log", "info"}}),
            ge::SUCCESS);
  EXPECT_EQ(FLAGS_static_model_ops_lower_limit, "5");
  EXPECT_EQ(FLAGS_log, "info");
  EXPECT_EQ(GetRawAppliedFlagNames().count("static_model_ops_lower_limit"), 1U);
  EXPECT_EQ(GetRawAppliedFlagOptions()["log"], "info");

  ResetRawState();
  flgs::GetUserOptions()["log"] = "warning";
  EXPECT_EQ(ApplyRawGeOptionsToFlags({{"log", "info"}}), ge::SUCCESS);
  EXPECT_EQ(GetRawAppliedFlagNames().count("log"), 0U);
}

TEST_F(AtcRawOptionsUTest, LoadRawGeOptionsReadsFileFiltersAndValidatesOptions) {
  std::map<std::string, std::string> raw_options;
  EXPECT_EQ(LoadRawGeOptions(raw_options), ge::SUCCESS);
  EXPECT_TRUE(raw_options.empty());

  FLAGS_raw_ge_options = WriteTempFile("load_raw_options", R"({
    "compile options": {
      "global": {
        "log": "info",
        "not.supported": "value"
      },
      "graph": {
        "ge.exec.static_model_ops_lower_limit": "9"
      }
    }
  })");
  EXPECT_EQ(LoadRawGeOptions(raw_options), ge::FAILED);

  raw_options.clear();
  FLAGS_raw_ge_options_ignore_unsupported = true;
  EXPECT_EQ(LoadRawGeOptions(raw_options), ge::SUCCESS);
  EXPECT_EQ(raw_options["log"], "info");
  EXPECT_EQ(raw_options[OPTION_STATIC_MODEL_OPS_LOWER_LIMIT], "9");
  EXPECT_EQ(raw_options.count("not.supported"), 0U);
}

TEST_F(AtcRawOptionsUTest, MergeRawGeOptionsSkipsCliHandledAndExplicitOptions) {
  std::map<std::string, std::string> options;
  MergeRawGeOptions({{OPTION_STATIC_MODEL_OPS_LOWER_LIMIT, "5"}}, options);
  EXPECT_EQ(options.count(OPTION_STATIC_MODEL_OPS_LOWER_LIMIT), 0U);

  flgs::GetUserOptions()[kRawUtCheckerFlag] = "cli";
  MergeRawGeOptions({{kRawUtCheckerOption, "raw"}}, options);
  EXPECT_EQ(options.count(kRawUtCheckerOption), 0U);

  flgs::GetUserOptions().clear();
  MergeRawGeOptions({{kRawUtCheckerOption, "raw"}}, options);
  EXPECT_EQ(options[kRawUtCheckerOption], "raw");
}
}  // namespace ge
