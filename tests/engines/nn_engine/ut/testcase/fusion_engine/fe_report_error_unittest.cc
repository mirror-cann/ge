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

class UTestFeReportError : public testing::Test {
 protected:
  static void SetUpTestCase() {}
  static void TearDownTestCase() {}
  void SetUp() {}
  void TearDown() {}
};

TEST_F(UTestFeReportError, TestReportErrorMessage_Success) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_COMPILE_FAILED;
  error_detail.arg_list.push_back("test_pass");
  error_detail.arg_list.push_back("test_type");
  
  ReportErrorMessage(error_detail);
}

TEST_F(UTestFeReportError, TestModifyArgsByErrorCode_InputOptionInvalid) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_INPUT_OPTION_INVALID;
  error_detail.arg_list.push_back("invalid_value");
  error_detail.arg_list.push_back("ge.op_compiler_cache_mode");
  error_detail.arg_list.push_back("invalid_reason");
  
  error_detail.ModifyArgsByErrorCode();
}

TEST_F(UTestFeReportError, TestToParamMap_ValidErrorCode) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_COMPILE_FAILED;
  error_detail.arg_list.push_back("test_pass");
  error_detail.arg_list.push_back("test_type");
  
  std::map<std::string, std::string> args_map;
  error_detail.ToParamMap(args_map);
  
  EXPECT_TRUE(args_map.size() > 0);
}

TEST_F(UTestFeReportError, TestToParamMap_InvalidErrorCode) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = "INVALID_ERROR_CODE";
  error_detail.arg_list.push_back("test_arg");
  
  std::map<std::string, std::string> args_map;
  error_detail.ToParamMap(args_map);
  
  EXPECT_TRUE(args_map.empty());
}

TEST_F(UTestFeReportError, TestReportErrorMessage_EmptyErrorCode) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = "";
  error_detail.arg_list.push_back("test_arg");
  
  ReportErrorMessage(error_detail);
}

TEST_F(UTestFeReportError, TestModifyArgsByErrorCode_EmptyArgList) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_INPUT_OPTION_INVALID;
  
  error_detail.ModifyArgsByErrorCode();
}

TEST_F(UTestFeReportError, TestModifyArgsByErrorCode_AiCoreNumOutOfRange) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_AICORENUM_OUT_OF_RANGE;
  error_detail.arg_list.push_back("invalid_value");
  error_detail.arg_list.push_back("ge.aicoreNum");
  error_detail.arg_list.push_back("1-32");
  
  error_detail.ModifyArgsByErrorCode();
}