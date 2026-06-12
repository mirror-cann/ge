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
#undef private
#undef protected

using namespace std;
using namespace testing;
using namespace fe;

class fe_report_error_stest : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

TEST_F(fe_report_error_stest, test_report_error_message_coverage80) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_COMPILE_FAILED;
  error_detail.arg_list.push_back("test_op_name");
  error_detail.arg_list.push_back("test_op_type");
  
  ReportErrorMessage(error_detail);
}

TEST_F(fe_report_error_stest, test_modify_args_by_error_code_coverage) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_AICORENUM_OUT_OF_RANGE;
  error_detail.arg_list.push_back("invalid_value");
  error_detail.arg_list.push_back("ge.aicoreNum");
  error_detail.arg_list.push_back("1-32");
  
  error_detail.ModifyArgsByErrorCode();
}

TEST_F(fe_report_error_stest, test_to_param_map_coverage) {
  ErrorMessageDetail error_detail;
  error_detail.error_code = EM_INPUT_OPTION_INVALID;
  error_detail.arg_list.push_back("test_arg");
  
  std::map<std::string, std::string> args_map;
  error_detail.ToParamMap(args_map);
}