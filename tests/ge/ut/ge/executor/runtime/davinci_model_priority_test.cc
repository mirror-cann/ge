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
#include <memory>
#include <vector>

#include "macro_utils/dt_public_scope.h"

#include "graph/load/model_manager/davinci_model.h"
#include "graph/model.h"
#include "depends/ascendcl/src/ascendcl_stub.h"

using namespace testing;
using namespace ge;

namespace {
class RecordingAclRuntimeStub : public AclRuntimeStub {
 public:
  explicit RecordingAclRuntimeStub(aclError ret) : ret_(ret) {}

  aclError aclrtSetStreamAttribute(aclrtStream stream, aclrtStreamAttr attr, aclrtStreamAttrValue *value) override {
    streams_.push_back(stream);
    attrs_.push_back(attr);
    priorities_.push_back(value == nullptr ? -1 : static_cast<int32_t>(value->streamPriority));
    return ret_;
  }

  aclError ret_;
  std::vector<aclrtStream> streams_;
  std::vector<aclrtStreamAttr> attrs_;
  std::vector<int32_t> priorities_;
};
}  // namespace

class UTEST_DavinciModel_StreamPriority : public testing::Test {
 protected:
  virtual void SetUp() {
    AclRuntimeStub::SetInstance(std::make_shared<AclRuntimeStub>());
  }
  virtual void TearDown() {
    AclRuntimeStub::Reset();
  }
};

TEST_F(UTEST_DavinciModel_StreamPriority, CollectOwnedStreamsEmpty) {
  DavinciModel model(0, nullptr);
  std::vector<aclrtStream> streams;
  model.CollectOwnedStreams(streams);
  EXPECT_TRUE(streams.empty());
}

TEST_F(UTEST_DavinciModel_StreamPriority, GetStreamPriorityDefault) {
  DavinciModel model(0, nullptr);
  uint32_t priority = 0;
  Status ret = model.GetStreamPriority(priority);
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_EQ(priority, 0);
}

TEST_F(UTEST_DavinciModel_StreamPriority, SetStreamPriorityNoStreams) {
  DavinciModel model(0, nullptr);
  Status ret = model.SetStreamPriority(5);
  EXPECT_EQ(ret, SUCCESS);
}

TEST_F(UTEST_DavinciModel_StreamPriority, SetAndGetStreamPriority) {
  DavinciModel model(0, nullptr);
  Status ret = model.SetStreamPriority(3);
  EXPECT_EQ(ret, SUCCESS);

  uint32_t priority = 0;
  ret = model.GetStreamPriority(priority);
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_EQ(priority, 3);
}

TEST_F(UTEST_DavinciModel_StreamPriority, PriorityReturnsCachedStreamPriority) {
  DavinciModel model(0, nullptr);
  EXPECT_EQ(model.Priority(), 0);

  EXPECT_EQ(model.SetStreamPriority(4), SUCCESS);
  EXPECT_EQ(model.Priority(), 4);
}

TEST_F(UTEST_DavinciModel_StreamPriority, SetStreamPriorityMultipleUpdates) {
  DavinciModel model(0, nullptr);
  EXPECT_EQ(model.SetStreamPriority(2), SUCCESS);
  EXPECT_EQ(model.SetStreamPriority(5), SUCCESS);
  EXPECT_EQ(model.SetStreamPriority(7), SUCCESS);

  uint32_t priority = 0;
  EXPECT_EQ(model.GetStreamPriority(priority), SUCCESS);
  EXPECT_EQ(priority, 7);
}

TEST_F(UTEST_DavinciModel_StreamPriority, GetStreamPriorityConst) {
  DavinciModel model(0, nullptr);
  model.SetStreamPriority(4);

  uint32_t priority = 0;
  Status ret = model.GetStreamPriority(priority);
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_EQ(priority, 4);
}

TEST_F(UTEST_DavinciModel_StreamPriority, SetStreamPriorityUpdatesAllOwnedStreams) {
  auto acl_runtime = std::make_shared<RecordingAclRuntimeStub>(ACL_SUCCESS);
  AclRuntimeStub::SetInstance(acl_runtime);
  DavinciModel model(0, nullptr);
  auto stream1 = reinterpret_cast<aclrtStream>(0x10);
  auto stream2 = reinterpret_cast<aclrtStream>(0x20);
  auto stream3 = reinterpret_cast<aclrtStream>(0x30);
  auto stream4 = reinterpret_cast<aclrtStream>(0x40);

  model.stream_list_ = {stream2, nullptr, stream1};
  model.all_hccl_stream_list_ = {stream3, nullptr, stream1};
  model.rt_head_stream_ = stream4;
  model.rt_entry_stream_ = stream2;
  model.rt_model_stream_ = stream3;
  model.is_inner_model_stream_ = true;

  EXPECT_EQ(model.SetStreamPriority(6), SUCCESS);

  EXPECT_THAT(acl_runtime->streams_, ElementsAre(stream1, stream2, stream3, stream4));
  EXPECT_THAT(acl_runtime->attrs_, Each(ACL_STREAM_ATTR_PRIORITY));
  EXPECT_THAT(acl_runtime->priorities_, Each(6));
  uint32_t priority = 0;
  EXPECT_EQ(model.GetStreamPriority(priority), SUCCESS);
  EXPECT_EQ(priority, 6);

  model.stream_list_.clear();
  model.all_hccl_stream_list_.clear();
  model.rt_head_stream_ = nullptr;
  model.rt_entry_stream_ = nullptr;
  model.rt_model_stream_ = nullptr;
  model.is_inner_model_stream_ = false;
}

TEST_F(UTEST_DavinciModel_StreamPriority, SetStreamPriorityReturnsFailedWhenRuntimeSetFailed) {
  auto acl_runtime = std::make_shared<RecordingAclRuntimeStub>(ACL_ERROR_RT_INTERNAL_ERROR);
  AclRuntimeStub::SetInstance(acl_runtime);
  DavinciModel model(0, nullptr);
  auto stream = reinterpret_cast<aclrtStream>(0x10);
  model.stream_list_ = {stream};

  EXPECT_EQ(model.SetStreamPriority(6), FAILED);

  EXPECT_THAT(acl_runtime->streams_, ElementsAre(stream));
  uint32_t priority = 0;
  EXPECT_EQ(model.GetStreamPriority(priority), SUCCESS);
  EXPECT_EQ(priority, 0);

  model.stream_list_.clear();
}

TEST_F(UTEST_DavinciModel_StreamPriority, SetStreamPriorityIgnoresRuntimeFeatureNotSupport) {
  auto acl_runtime = std::make_shared<RecordingAclRuntimeStub>(ACL_ERROR_RT_FEATURE_NOT_SUPPORT);
  AclRuntimeStub::SetInstance(acl_runtime);
  DavinciModel model(0, nullptr);
  auto stream = reinterpret_cast<aclrtStream>(0x10);
  model.stream_list_ = {stream};

  EXPECT_EQ(model.SetStreamPriority(6), SUCCESS);

  EXPECT_THAT(acl_runtime->streams_, ElementsAre(stream));
  EXPECT_THAT(acl_runtime->attrs_, Each(ACL_STREAM_ATTR_PRIORITY));
  EXPECT_THAT(acl_runtime->priorities_, Each(6));
  uint32_t priority = 0;
  EXPECT_EQ(model.GetStreamPriority(priority), SUCCESS);
  EXPECT_EQ(priority, 6);

  model.stream_list_.clear();
}
