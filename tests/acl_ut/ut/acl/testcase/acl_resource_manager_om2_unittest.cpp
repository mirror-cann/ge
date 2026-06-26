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
#include <gmock/gmock.h>

#include "acl/acl_base.h"
#include "model/model_desc_internal.h"
#define protected public
#define private public
#include "model/acl_resource_manager_om2.h"
#undef private
#undef protected
#include "model/acl_model_router.h"
#include "acl_stub.h"

#include <memory>
#include <vector>

using namespace std;
using namespace testing;
using namespace acl;

// Helper functions for IsOm2Model mock
namespace {
ge::Status IsOm2ModelFromFile(const char *file_path, bool &is_support) {
  (void)file_path;
  is_support = true;
  return ge::SUCCESS;
}

ge::Status IsOm2ModelFromData(const void *data, size_t size, bool &is_support) {
  constexpr size_t kFileMagicSize = 4U;
  if (data == nullptr || size < kFileMagicSize) {
    is_support = false;
    return ge::FAILED;
  }
  constexpr uint8_t kMagic[] = {0x50, 0x4B, 0x03, 0x04};
  is_support = std::memcmp(data, kMagic, kFileMagicSize) == 0;
  return ge::SUCCESS;
}

ge::Status IsNotOm2ModelFromFile(const char *file_path, bool &is_support) {
  (void)file_path;
  is_support = false;
  return ge::SUCCESS;
}

ge::Status IsNotOm2ModelFromData(const void *data, size_t size, bool &is_support) {
  (void)data;
  (void)size;
  is_support = false;
  return ge::SUCCESS;
}
}  // namespace

class UTEST_ACL_Resource_Manager_Om2 : public testing::Test {
 protected:
  virtual void SetUp() {
    MockFunctionTest::aclStubInstance().ResetToDefaultMock();
  }
  virtual void TearDown() {
    // 单例测试，需要恢复初始状态
    auto &instance = acl::AclResourceManagerOm2::GetInstance();
    instance.om2ExecutorMap_.clear();
    instance.om2ExecutorMap_[0U] = nullptr;  // 恢复初始状态
    instance.bundleInfos_.clear();
    instance.bundleInnerIds_.clear();
    Mock::VerifyAndClear((void *)(&MockFunctionTest::aclStubInstance()));
  }
};

// ============================================================================
// P0 优先级测试用例：核心功能测试
// ============================================================================

// 2.1 模型执行器管理测试（7个用例）

TEST_F(UTEST_ACL_Resource_Manager_Om2, AddOm2Executor_Success) {
  auto executor = std::unique_ptr<gert::Om2ModelExecutor>(new (std::nothrow) gert::Om2ModelExecutor);
  uint32_t modelId = 0;

  acl::AclResourceManagerOm2::GetInstance().AddOm2Executor(modelId, std::move(executor));

  EXPECT_GT(modelId, 0U);
  EXPECT_NE(acl::AclResourceManagerOm2::GetInstance().GetOm2Executor(modelId), nullptr);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, GetOm2Executor_Success) {
  auto executor = std::unique_ptr<gert::Om2ModelExecutor>(new (std::nothrow) gert::Om2ModelExecutor);
  uint32_t modelId = 0;
  acl::AclResourceManagerOm2::GetInstance().AddOm2Executor(modelId, std::move(executor));

  auto retrieved = acl::AclResourceManagerOm2::GetInstance().GetOm2Executor(modelId);

  EXPECT_NE(retrieved, nullptr);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, GetOm2Executor_NotFound) {
  auto retrieved = acl::AclResourceManagerOm2::GetInstance().GetOm2Executor(999U);

  EXPECT_EQ(retrieved, nullptr);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, GetOpDescInfo_NotFound) {
  ge::OpDescInfo opDescInfo;

  aclError ret = acl::AclResourceManagerOm2::GetInstance().GetOpDescInfo(0U, 0U, 0U, opDescInfo);

  EXPECT_EQ(ret, ACL_ERROR_GE_FAILURE);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, DeleteOm2Executor_Success) {
  auto executor = std::unique_ptr<gert::Om2ModelExecutor>(new (std::nothrow) gert::Om2ModelExecutor);
  uint32_t modelId = 0;
  acl::AclResourceManagerOm2::GetInstance().AddOm2Executor(modelId, std::move(executor));

  aclError ret = acl::AclResourceManagerOm2::GetInstance().DeleteOm2Executor(modelId);

  EXPECT_EQ(ret, ACL_SUCCESS);
  EXPECT_EQ(acl::AclResourceManagerOm2::GetInstance().GetOm2Executor(modelId), nullptr);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, DeleteOm2Executor_NotFound) {
  aclError ret = acl::AclResourceManagerOm2::GetInstance().DeleteOm2Executor(999U);

  EXPECT_EQ(ret, ACL_ERROR_GE_EXEC_MODEL_ID_INVALID);
}

// 2.2 模型类型判断测试（3个用例）

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelById_True) {
  auto executor = std::unique_ptr<gert::Om2ModelExecutor>(new (std::nothrow) gert::Om2ModelExecutor);
  uint32_t modelId = 0;
  acl::AclResourceManagerOm2::GetInstance().AddOm2Executor(modelId, std::move(executor));

  bool result = acl::AclResourceManagerOm2::GetInstance().IsOm2ModelById(modelId);

  EXPECT_TRUE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelById_False_NotExist) {
  bool result = acl::AclResourceManagerOm2::GetInstance().IsOm2ModelById(999U);

  EXPECT_FALSE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelById_False_ZeroId) {
  bool result = acl::AclResourceManagerOm2::GetInstance().IsOm2ModelById(0U);

  EXPECT_FALSE(result);
}

// 2.3 Bundle管理测试（4个用例）

TEST_F(UTEST_ACL_Resource_Manager_Om2, SetBundleInfo_Success) {
  acl::BundleModelInfo bundleInfo;
  bundleInfo.isInit = true;
  bundleInfo.varSize = 1024;
  bundleInfo.bundleModelSize = 2048;

  aclError ret = acl::AclResourceManagerOm2::GetInstance().SetBundleInfo(1U, bundleInfo);

  EXPECT_EQ(ret, ACL_SUCCESS);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, GetBundleInfo_Success) {
  acl::BundleModelInfo bundleInfo;
  bundleInfo.isInit = true;
  bundleInfo.varSize = 1024;
  bundleInfo.bundleModelSize = 2048;
  acl::AclResourceManagerOm2::GetInstance().SetBundleInfo(1U, bundleInfo);

  acl::BundleModelInfo retrieved;
  aclError ret = acl::AclResourceManagerOm2::GetInstance().GetBundleInfo(1U, retrieved);

  EXPECT_EQ(ret, ACL_SUCCESS);
  EXPECT_EQ(retrieved.isInit, true);
  EXPECT_EQ(retrieved.varSize, 1024U);
  EXPECT_EQ(retrieved.bundleModelSize, 2048U);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, GetBundleInfo_NotFound) {
  acl::BundleModelInfo retrieved;
  aclError ret = acl::AclResourceManagerOm2::GetInstance().GetBundleInfo(999U, retrieved);

  EXPECT_EQ(ret, ACL_ERROR_INVALID_BUNDLE_MODEL_ID);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, DeleteBundleInfo_Success) {
  acl::BundleModelInfo bundleInfo;
  bundleInfo.isInit = true;
  acl::AclResourceManagerOm2::GetInstance().SetBundleInfo(1U, bundleInfo);

  acl::AclResourceManagerOm2::GetInstance().DeleteBundleInfo(1U);

  acl::BundleModelInfo retrieved;
  aclError ret = acl::AclResourceManagerOm2::GetInstance().GetBundleInfo(1U, retrieved);
  EXPECT_EQ(ret, ACL_ERROR_INVALID_BUNDLE_MODEL_ID);
}

// ============================================================================
// P1 优先级测试用例：完整功能覆盖和边界条件测试
// ============================================================================

// 3.2 Bundle管理扩展测试（7个用例）

TEST_F(UTEST_ACL_Resource_Manager_Om2, DeleteBundleInfo_NotExist) {
  // 删除不存在的bundleId，应该不报错
  acl::AclResourceManagerOm2::GetInstance().DeleteBundleInfo(999U);

  // 验证没有异常
  EXPECT_TRUE(true);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, AddBundleSubmodelId_Success) {
  uint32_t bundleId = 1U;
  uint32_t modelId = 100U;

  acl::AclResourceManagerOm2::GetInstance().AddBundleSubmodelId(bundleId, modelId);

  EXPECT_TRUE(acl::AclResourceManagerOm2::GetInstance().IsBundleInnerId(modelId));
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, DeleteBundleSubmodelId_Success) {
  uint32_t bundleId = 1U;
  uint32_t modelId = 100U;
  acl::AclResourceManagerOm2::GetInstance().AddBundleSubmodelId(bundleId, modelId);

  acl::AclResourceManagerOm2::GetInstance().DeleteBundleSubmodelId(bundleId, modelId);

  EXPECT_FALSE(acl::AclResourceManagerOm2::GetInstance().IsBundleInnerId(modelId));
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsBundleInnerId_True) {
  uint32_t bundleId = 1U;
  uint32_t modelId = 100U;
  acl::AclResourceManagerOm2::GetInstance().AddBundleSubmodelId(bundleId, modelId);

  bool result = acl::AclResourceManagerOm2::GetInstance().IsBundleInnerId(modelId);

  EXPECT_TRUE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsBundleInnerId_False) {
  bool result = acl::AclResourceManagerOm2::GetInstance().IsBundleInnerId(999U);

  EXPECT_FALSE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2BundleById_True) {
  acl::BundleModelInfo bundleInfo;
  bundleInfo.isInit = true;
  acl::AclResourceManagerOm2::GetInstance().SetBundleInfo(1U, bundleInfo);

  bool result = acl::AclResourceManagerOm2::GetInstance().IsOm2BundleById(1U);

  EXPECT_TRUE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2BundleById_False) {
  bool result = acl::AclResourceManagerOm2::GetInstance().IsOm2BundleById(999U);

  EXPECT_FALSE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, SetBundleInfo_WithSubmodels) {
  acl::BundleModelInfo bundleInfo;
  bundleInfo.isInit = true;
  bundleInfo.loadedSubModelIdSet.insert(100U);
  bundleInfo.loadedSubModelIdSet.insert(101U);
  bundleInfo.loadedSubModelIdSet.insert(102U);

  aclError ret = acl::AclResourceManagerOm2::GetInstance().SetBundleInfo(1U, bundleInfo);

  EXPECT_EQ(ret, ACL_SUCCESS);
  EXPECT_TRUE(acl::AclResourceManagerOm2::GetInstance().IsBundleInnerId(100U));
  EXPECT_TRUE(acl::AclResourceManagerOm2::GetInstance().IsBundleInnerId(101U));
  EXPECT_TRUE(acl::AclResourceManagerOm2::GetInstance().IsBundleInnerId(102U));
}

// ============================================================================
// 模型类型判断扩展测试（IsOm2ModelByPath, IsOm2ModelByData, IsOm2ModelByConfig）
// ============================================================================

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByPath_NullPath) {
  bool result = false;
  AclIsOm2ModelByPath(nullptr, &result);

  EXPECT_FALSE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByData_NullData) {
  bool result = false;
  AclIsOm2ModelByData(nullptr, 100, &result);

  EXPECT_FALSE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByData_ZeroSize) {
  char data[] = "test";
  bool result = false;
  AclIsOm2ModelByData(data, 0, &result);

  EXPECT_FALSE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByConfig_NullHandle) {
  bool result = false;
  AclIsOm2ModelByConfig(nullptr, &result);

  EXPECT_FALSE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByConfig_EmptyConfig) {
  aclmdlConfigHandle handle;
  handle.loadPath = "";
  handle.mdlAddr = nullptr;
  handle.mdlSize = 0;

  bool result = false;
  AclIsOm2ModelByConfig(&handle, &result);

  EXPECT_FALSE(result);
}

// ============================================================================
// 多个执行器管理测试
// ============================================================================

TEST_F(UTEST_ACL_Resource_Manager_Om2, MultipleExecutors_AddAndRetrieve) {
  std::vector<uint32_t> modelIds;

  for (int i = 0; i < 5; ++i) {
    auto executor = std::unique_ptr<gert::Om2ModelExecutor>(new (std::nothrow) gert::Om2ModelExecutor);
    uint32_t modelId = 0;
    acl::AclResourceManagerOm2::GetInstance().AddOm2Executor(modelId, std::move(executor));
    modelIds.push_back(modelId);
  }

  for (uint32_t modelId : modelIds) {
    EXPECT_NE(acl::AclResourceManagerOm2::GetInstance().GetOm2Executor(modelId), nullptr);
  }
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, MultipleExecutors_DeleteAll) {
  std::vector<uint32_t> modelIds;

  for (int i = 0; i < 3; ++i) {
    auto executor = std::unique_ptr<gert::Om2ModelExecutor>(new (std::nothrow) gert::Om2ModelExecutor);
    uint32_t modelId = 0;
    acl::AclResourceManagerOm2::GetInstance().AddOm2Executor(modelId, std::move(executor));
    modelIds.push_back(modelId);
  }

  for (uint32_t modelId : modelIds) {
    aclError ret = acl::AclResourceManagerOm2::GetInstance().DeleteOm2Executor(modelId);
    EXPECT_EQ(ret, ACL_SUCCESS);
  }

  for (uint32_t modelId : modelIds) {
    EXPECT_EQ(acl::AclResourceManagerOm2::GetInstance().GetOm2Executor(modelId), nullptr);
  }
}

// ============================================================================
// Bundle和执行器交互测试
// ============================================================================

TEST_F(UTEST_ACL_Resource_Manager_Om2, BundleWithExecutors) {
  // 创建多个执行器
  std::vector<uint32_t> modelIds;
  for (int i = 0; i < 3; ++i) {
    auto executor = std::unique_ptr<gert::Om2ModelExecutor>(new (std::nothrow) gert::Om2ModelExecutor);
    uint32_t modelId = 0;
    acl::AclResourceManagerOm2::GetInstance().AddOm2Executor(modelId, std::move(executor));
    modelIds.push_back(modelId);
  }

  // 创建Bundle并关联执行器
  acl::BundleModelInfo bundleInfo;
  bundleInfo.isInit = true;
  for (uint32_t modelId : modelIds) {
    bundleInfo.loadedSubModelIdSet.insert(modelId);
  }

  aclError ret = acl::AclResourceManagerOm2::GetInstance().SetBundleInfo(1U, bundleInfo);
  EXPECT_EQ(ret, ACL_SUCCESS);

  // 验证所有执行器都被标记为Bundle内部ID
  for (uint32_t modelId : modelIds) {
    EXPECT_TRUE(acl::AclResourceManagerOm2::GetInstance().IsBundleInnerId(modelId));
  }
}

// ============================================================================
// 模型类型判断正向测试用例（IsOm2ModelByPath, IsOm2ModelByData, IsOm2ModelByConfig）
// ============================================================================

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByPath_ValidOm2Model) {
  EXPECT_CALL(MockFunctionTest::aclStubInstance(), IsOm2Model(_, _)).WillOnce(Invoke(IsOm2ModelFromFile));

  bool result = false;
  AclIsOm2ModelByPath("/path/to/om2_model.om", &result);

  EXPECT_TRUE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByPath_ValidNonOm2Model) {
  EXPECT_CALL(MockFunctionTest::aclStubInstance(), IsOm2Model(_, _)).WillOnce(Invoke(IsNotOm2ModelFromFile));

  bool result = false;
  AclIsOm2ModelByPath("/path/to/non_om2_model.om", &result);

  EXPECT_FALSE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByPath_EmptyString) {
  bool result = false;
  AclIsOm2ModelByPath("", &result);

  EXPECT_FALSE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByData_ValidOm2Model) {
  uint8_t om2ModelData[] = {0x50, 0x4B, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00};
  EXPECT_CALL(MockFunctionTest::aclStubInstance(), IsOm2Model(_, _, _)).WillOnce(Invoke(IsOm2ModelFromData));

  bool result = false;
  AclIsOm2ModelByData(om2ModelData, sizeof(om2ModelData), &result);

  EXPECT_TRUE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByData_ValidNonOm2Model) {
  char nonOm2ModelData[] = "non_om2_model_data";
  EXPECT_CALL(MockFunctionTest::aclStubInstance(), IsOm2Model(_, _, _)).WillOnce(Invoke(IsNotOm2ModelFromData));

  bool result = false;
  AclIsOm2ModelByData(nonOm2ModelData, sizeof(nonOm2ModelData), &result);

  EXPECT_FALSE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByConfig_WithValidOm2Path) {
  EXPECT_CALL(MockFunctionTest::aclStubInstance(), IsOm2Model(_, _)).WillOnce(Invoke(IsOm2ModelFromFile));

  aclmdlConfigHandle handle;
  handle.loadPath = "/path/to/om2_model.om";
  handle.mdlAddr = nullptr;
  handle.mdlSize = 0;

  bool result = false;
  AclIsOm2ModelByConfig(&handle, &result);

  EXPECT_TRUE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByConfig_WithValidNonOm2Path) {
  EXPECT_CALL(MockFunctionTest::aclStubInstance(), IsOm2Model(_, _)).WillOnce(Invoke(IsNotOm2ModelFromFile));

  aclmdlConfigHandle handle;
  handle.loadPath = "/path/to/non_om2_model.om";
  handle.mdlAddr = nullptr;
  handle.mdlSize = 0;

  bool result = false;
  AclIsOm2ModelByConfig(&handle, &result);

  EXPECT_FALSE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByConfig_WithValidOm2Data) {
  uint8_t om2ModelData[] = {0x50, 0x4B, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00};
  EXPECT_CALL(MockFunctionTest::aclStubInstance(), IsOm2Model(_, _, _)).WillOnce(Invoke(IsOm2ModelFromData));

  aclmdlConfigHandle handle;
  handle.loadPath = "";
  handle.mdlAddr = om2ModelData;
  handle.mdlSize = sizeof(om2ModelData);

  bool result = false;
  AclIsOm2ModelByConfig(&handle, &result);

  EXPECT_TRUE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByConfig_WithValidNonOm2Data) {
  char nonOm2ModelData[] = "non_om2_model_data";
  EXPECT_CALL(MockFunctionTest::aclStubInstance(), IsOm2Model(_, _, _)).WillOnce(Invoke(IsNotOm2ModelFromData));

  aclmdlConfigHandle handle;
  handle.loadPath = "";
  handle.mdlAddr = nonOm2ModelData;
  handle.mdlSize = sizeof(nonOm2ModelData);

  bool result = false;
  AclIsOm2ModelByConfig(&handle, &result);

  EXPECT_FALSE(result);
}

TEST_F(UTEST_ACL_Resource_Manager_Om2, IsOm2ModelByConfig_PathTakesPrecedence) {
  // 路径优先于数据，所以只应该调用路径版本的 IsOm2Model
  EXPECT_CALL(MockFunctionTest::aclStubInstance(), IsOm2Model(_, _)).WillOnce(Invoke(IsOm2ModelFromFile));

  uint8_t om2ModelData[] = {0x50, 0x4B, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00};
  aclmdlConfigHandle handle;
  handle.loadPath = "/path/to/om2_model.om";
  handle.mdlAddr = om2ModelData;
  handle.mdlSize = sizeof(om2ModelData);

  bool result = false;
  AclIsOm2ModelByConfig(&handle, &result);

  EXPECT_TRUE(result);
}
