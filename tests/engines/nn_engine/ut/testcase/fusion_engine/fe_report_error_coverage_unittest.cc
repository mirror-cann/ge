/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include "fe_llt_utils.h"
#define protected public
#define private public
#include "common/fe_report_error.h"
#include "common/fe_error_code.h"
#include "graph/ge_context.h"
#include "base/err_msg.h"
#undef private
#undef protected

using namespace std;
using namespace testing;
using namespace fe;

class UTestFeReportErrorCoverage : public testing::Test {
 protected:
  static void SetUpTestCase() {}
  static void TearDownTestCase() {}
  void SetUp() {}
  void TearDown() {}
};

TEST_F(UTestFeReportErrorCoverage, TestReportErrorMessage_AllErrorCodes) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_COMPILE_FAILED;
  error_detail.arg_list.push_back("test_pass");
  error_detail.arg_list.push_back("test_type");
  ReportErrorMessage(error_detail);
}

TEST_F(UTestFeReportErrorCoverage, TestReportErrorMessage_EnvironmentVariableFailed) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_ENVIRONMENT_VARIABLE_FAILED;
  error_detail.arg_list.push_back("test_value");
  error_detail.arg_list.push_back("test_env");
  error_detail.arg_list.push_back("test_reason");
  ReportErrorMessage(error_detail);
}

TEST_F(UTestFeReportErrorCoverage, TestReportErrorMessage_InvalidContent) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_INVALID_CONTENT;
  error_detail.arg_list.push_back("test_parameter");
  error_detail.arg_list.push_back("test_filepath");
  error_detail.arg_list.push_back("test_reason");
  ReportErrorMessage(error_detail);
}

TEST_F(UTestFeReportErrorCoverage, TestReportErrorMessage_RunPassFailed) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_RUN_PASS_FAILED;
  error_detail.arg_list.push_back("test_pass_name");
  error_detail.arg_list.push_back("test_pass_type");
  ReportErrorMessage(error_detail);
}

TEST_F(UTestFeReportErrorCoverage, TestReportErrorMessage_InputOptionInvalid) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_INPUT_OPTION_INVALID;
  error_detail.arg_list.push_back("test_value");
  error_detail.arg_list.push_back("test_parameter");
  error_detail.arg_list.push_back("test_reason");
  ReportErrorMessage(error_detail);
}

TEST_F(UTestFeReportErrorCoverage, TestReportErrorMessage_AicoreNumOutOfRange) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_AICORENUM_OUT_OF_RANGE;
  error_detail.arg_list.push_back("test_value");
  error_detail.arg_list.push_back("test_parameter");
  error_detail.arg_list.push_back("test_range");
  ReportErrorMessage(error_detail);
}

TEST_F(UTestFeReportErrorCoverage, TestReportErrorMessage_OpenFileFailed) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_OPEN_FILE_FAILED;
  error_detail.arg_list.push_back("test_file_name");
  ReportErrorMessage(error_detail);
}

TEST_F(UTestFeReportErrorCoverage, TestReportErrorMessage_ReadFileFailed) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_READ_FILE_FAILED;
  error_detail.arg_list.push_back("test_file_name");
  error_detail.arg_list.push_back("test_reason");
  ReportErrorMessage(error_detail);
}

TEST_F(UTestFeReportErrorCoverage, TestModifyArgsByErrorCode_AllScenarios) {
  ErrorMessageDetail error_detail;
  
  error_detail.error_code = EM_COMPILE_FAILED;
  error_detail.arg_list.push_back("test_arg");
  error_detail.ModifyArgsByErrorCode();
  
  error_detail.error_code = EM_INPUT_OPTION_INVALID;
  error_detail.arg_list.clear();
  error_detail.arg_list.push_back("test_value");
  error_detail.arg_list.push_back("ge.op_compiler_cache_mode");
  error_detail.arg_list.push_back("test_reason");
  error_detail.ModifyArgsByErrorCode();
  
  error_detail.error_code = EM_AICORENUM_OUT_OF_RANGE;
  error_detail.arg_list.clear();
  error_detail.arg_list.push_back("test_value");
  error_detail.arg_list.push_back("test_parameter");
  error_detail.arg_list.push_back("test_range");
  error_detail.ModifyArgsByErrorCode();
}

TEST_F(UTestFeReportErrorCoverage, TestToParamMap_EmptyArgList) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_COMPILE_FAILED;
  
  std::map<std::string, std::string> args_map;
  error_detail.ToParamMap(args_map);
  EXPECT_TRUE(args_map.empty());
}

TEST_F(UTestFeReportErrorCoverage, TestToParamMap_MoreArgsThanParams) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_COMPILE_FAILED;
  error_detail.arg_list.push_back("arg1");
  error_detail.arg_list.push_back("arg2");
  error_detail.arg_list.push_back("arg3");
  error_detail.arg_list.push_back("arg4");
  
  std::map<std::string, std::string> args_map;
  error_detail.ToParamMap(args_map);
  EXPECT_EQ(args_map.size(), 2);
}