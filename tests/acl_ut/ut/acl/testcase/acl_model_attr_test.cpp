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
#include <gmock/gmock.h>

#include "acl/acl_mdl.h"
#include "acl_stub.h"

using namespace testing;
using namespace std;

class UTEST_ACL_ModelAttr : public testing::Test {
 protected:
  virtual void SetUp() {
    MockFunctionTest::aclStubInstance().ResetToDefaultMock();
  }
  virtual void TearDown() {
    Mock::VerifyAndClear((void *)(&MockFunctionTest::aclStubInstance()));
  }
};

TEST_F(UTEST_ACL_ModelAttr, SetAttrNullAttrValue) {
  uint32_t modelId = 100;
  aclError ret = aclmdlSetAttribute(modelId, ACL_MDL_ATTR_PRIORITY_INT32, nullptr);
  EXPECT_EQ(ret, ACL_ERROR_INVALID_PARAM);
}

TEST_F(UTEST_ACL_ModelAttr, GetAttrNullAttrValue) {
  uint32_t modelId = 100;
  aclError ret = aclmdlGetAttribute(modelId, ACL_MDL_ATTR_PRIORITY_INT32, nullptr);
  EXPECT_EQ(ret, ACL_ERROR_INVALID_PARAM);
}

TEST_F(UTEST_ACL_ModelAttr, AttrValueUnionLayout) {
  aclmdlAttrValue_t attrValue = {};

  EXPECT_EQ(sizeof(attrValue), sizeof(uint32_t) * 8U);
  attrValue.reserved[0] = 3U;
  EXPECT_EQ(attrValue.mdlPriority, 3U);
  attrValue.mdlPriority = 5U;
  EXPECT_EQ(attrValue.reserved[0], 5U);
}

TEST_F(UTEST_ACL_ModelAttr, SetAttrUnsupportedAttr) {
  uint32_t modelId = 100;
  aclmdlAttrValue_t attrValue = {};
  aclmdlAttr invalidAttr = static_cast<aclmdlAttr>(999);
  aclError ret = aclmdlSetAttribute(modelId, invalidAttr, &attrValue);
  EXPECT_EQ(ret, ACL_ERROR_INVALID_PARAM);
}

TEST_F(UTEST_ACL_ModelAttr, GetAttrUnsupportedAttr) {
  uint32_t modelId = 100;
  aclmdlAttrValue_t attrValue = {};
  aclmdlAttr invalidAttr = static_cast<aclmdlAttr>(999);
  aclError ret = aclmdlGetAttribute(modelId, invalidAttr, &attrValue);
  EXPECT_EQ(ret, ACL_ERROR_INVALID_PARAM);
}

TEST_F(UTEST_ACL_ModelAttr, SetAttrPriorityOutOfRangeHigh) {
  uint32_t modelId = 100;
  aclmdlAttrValue_t attrValue = {};
  attrValue.mdlPriority = 8U;
  aclError ret = aclmdlSetAttribute(modelId, ACL_MDL_ATTR_PRIORITY_INT32, &attrValue);
  EXPECT_EQ(ret, ACL_ERROR_INVALID_PARAM);
}

TEST_F(UTEST_ACL_ModelAttr, SetAttrPriorityDispatchSuccess) {
  uint32_t modelId = 100;
  aclmdlAttrValue_t attrValue = {};
  attrValue.mdlPriority = 3U;

  EXPECT_CALL(MockFunctionTest::aclStubInstance(), SetModelStreamPriority(modelId, 3)).WillOnce(Return(ge::SUCCESS));
  aclError ret = aclmdlSetAttribute(modelId, ACL_MDL_ATTR_PRIORITY_INT32, &attrValue);
  EXPECT_EQ(ret, ACL_SUCCESS);
}

TEST_F(UTEST_ACL_ModelAttr, GetAttrPriorityWriteBackSuccess) {
  uint32_t modelId = 100;
  aclmdlAttrValue_t attrValue = {};

  EXPECT_CALL(MockFunctionTest::aclStubInstance(), GetModelStreamPriority(modelId, _))
      .WillOnce(DoAll(SetArgReferee<1>(5), Return(ge::SUCCESS)));
  aclError ret = aclmdlGetAttribute(modelId, ACL_MDL_ATTR_PRIORITY_INT32, &attrValue);
  EXPECT_EQ(ret, ACL_SUCCESS);
  EXPECT_EQ(attrValue.mdlPriority, 5U);
}

TEST_F(UTEST_ACL_ModelAttr, SetAttrPriorityInvalidModelId) {
  uint32_t modelId = 100;
  aclmdlAttrValue_t attrValue = {};
  attrValue.mdlPriority = 7U;

  EXPECT_CALL(MockFunctionTest::aclStubInstance(), SetModelStreamPriority(modelId, 7))
      .WillOnce(Return(static_cast<ge::Status>(ACL_ERROR_GE_EXEC_MODEL_ID_INVALID)));
  aclError ret = aclmdlSetAttribute(modelId, ACL_MDL_ATTR_PRIORITY_INT32, &attrValue);
  EXPECT_EQ(ret, ACL_ERROR_GE_EXEC_MODEL_ID_INVALID);
}

TEST_F(UTEST_ACL_ModelAttr, GetAttrPriorityInvalidModelId) {
  uint32_t modelId = 100;
  aclmdlAttrValue_t attrValue = {};

  EXPECT_CALL(MockFunctionTest::aclStubInstance(), GetModelStreamPriority(modelId, _))
      .WillOnce(Return(static_cast<ge::Status>(ACL_ERROR_GE_EXEC_MODEL_ID_INVALID)));
  aclError ret = aclmdlGetAttribute(modelId, ACL_MDL_ATTR_PRIORITY_INT32, &attrValue);
  EXPECT_EQ(ret, ACL_ERROR_GE_EXEC_MODEL_ID_INVALID);
}

TEST_F(UTEST_ACL_ModelAttr, SetAttrPriorityGeFailureMapsToGeFailure) {
  uint32_t modelId = 100;
  aclmdlAttrValue_t attrValue = {};
  attrValue.mdlPriority = 3U;

  EXPECT_CALL(MockFunctionTest::aclStubInstance(), SetModelStreamPriority(modelId, 3)).WillOnce(Return(ge::FAILED));
  aclError ret = aclmdlSetAttribute(modelId, ACL_MDL_ATTR_PRIORITY_INT32, &attrValue);
  EXPECT_EQ(ret, ACL_ERROR_GE_FAILURE);
}

TEST_F(UTEST_ACL_ModelAttr, GetAttrPriorityGeFailureMapsToGeFailure) {
  uint32_t modelId = 100;
  aclmdlAttrValue_t attrValue = {};

  EXPECT_CALL(MockFunctionTest::aclStubInstance(), GetModelStreamPriority(modelId, _)).WillOnce(Return(ge::FAILED));
  aclError ret = aclmdlGetAttribute(modelId, ACL_MDL_ATTR_PRIORITY_INT32, &attrValue);
  EXPECT_EQ(ret, ACL_ERROR_GE_FAILURE);
}
