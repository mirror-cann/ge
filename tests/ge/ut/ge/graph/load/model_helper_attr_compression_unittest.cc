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

#include <map>
#include <string>

#include "framework/common/helper/model_helper.h"
#include "framework/common/types.h"

namespace ge {
namespace {

class UtestModelHelperAttrCompression : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

class TestableModelHelper : public ModelHelper {
 public:
  bool TestShouldCompress() const {
    return ShouldCompress();
  }

  const char *TestGetCompressionModeString() const {
    return attr_compression_enabled_ ? "enable" : "disable";
  }
};

TEST_F(UtestModelHelperAttrCompression, AttrCompression_EnabledWithOffline_ShouldCompress) {
  TestableModelHelper helper;
  helper.SetSaveMode(true);
  helper.SetAttrCompressionEnabled(true);
  EXPECT_TRUE(helper.TestShouldCompress());
  EXPECT_STREQ(helper.TestGetCompressionModeString(), "enable");
}

TEST_F(UtestModelHelperAttrCompression, AttrCompression_EnabledWithOnline_ShouldNotCompress) {
  TestableModelHelper helper;
  helper.SetSaveMode(false);
  helper.SetAttrCompressionEnabled(true);
  EXPECT_FALSE(helper.TestShouldCompress());
  EXPECT_STREQ(helper.TestGetCompressionModeString(), "enable");
}

TEST_F(UtestModelHelperAttrCompression, AttrCompression_DisabledWithOffline_ShouldNotCompress) {
  TestableModelHelper helper;
  helper.SetSaveMode(true);
  helper.SetAttrCompressionEnabled(false);
  EXPECT_FALSE(helper.TestShouldCompress());
  EXPECT_STREQ(helper.TestGetCompressionModeString(), "disable");
}

TEST_F(UtestModelHelperAttrCompression, AttrCompression_DisabledWithOnline_ShouldNotCompress) {
  TestableModelHelper helper;
  helper.SetSaveMode(false);
  helper.SetAttrCompressionEnabled(false);
  EXPECT_FALSE(helper.TestShouldCompress());
  EXPECT_STREQ(helper.TestGetCompressionModeString(), "disable");
}

TEST_F(UtestModelHelperAttrCompression, AttrCompression_ConfigureFromOptions_ValidValues) {
  TestableModelHelper helper;
  EXPECT_EQ(helper.ConfigureAttrCompressionMode("true"), SUCCESS);
  EXPECT_STREQ(helper.TestGetCompressionModeString(), "enable");
  EXPECT_EQ(helper.ConfigureAttrCompressionMode("false"), SUCCESS);
  EXPECT_STREQ(helper.TestGetCompressionModeString(), "disable");
}

TEST_F(UtestModelHelperAttrCompression, AttrCompression_ConfigureFromOptions_OtherInvalidValues) {
  TestableModelHelper helper;
  EXPECT_EQ(helper.ConfigureAttrCompressionMode("enable"), PARAM_INVALID);
  EXPECT_EQ(helper.ConfigureAttrCompressionMode("disable"), PARAM_INVALID);
  EXPECT_EQ(helper.ConfigureAttrCompressionMode("1"), PARAM_INVALID);
  EXPECT_EQ(helper.ConfigureAttrCompressionMode("0"), PARAM_INVALID);
}

TEST_F(UtestModelHelperAttrCompression, AttrCompression_ConfigureFromOptions_EmptyOptions) {
  TestableModelHelper helper;
  std::map<std::string, std::string> options;
  EXPECT_EQ(helper.ConfigureAttrCompressionMode(""), PARAM_INVALID);
  EXPECT_STREQ(helper.TestGetCompressionModeString(), "enable");
}

}  // namespace
}  // namespace ge
