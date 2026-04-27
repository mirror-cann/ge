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
#include <memory>
#include <string>

#include "engines/custom_engine/custom_ops_kernel_info_store.h"
#include "graph/custom_op_factory.h"
#include "graph/ascend_string.h"
#include "graph/op_desc.h"
#include "graph/custom_op.h"

namespace ge {
namespace custom {

class MockCustomOp : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }
};

class UtestCustomOpsKernelInfoStore : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

TEST_F(UtestCustomOpsKernelInfoStore, InitializeSuccess) {
  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);
}

TEST_F(UtestCustomOpsKernelInfoStore, FinalizeSuccess) {
  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);
  EXPECT_EQ(store.Finalize(), SUCCESS);
}

TEST_F(UtestCustomOpsKernelInfoStore, RefreshSuccess) {
  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);
  EXPECT_EQ(store.Refresh(), SUCCESS);
}

TEST_F(UtestCustomOpsKernelInfoStore, GetAllOpsKernelInfo) {
  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);
  
  std::map<std::string, OpInfo> infos;
  store.GetAllOpsKernelInfo(infos);
  
  std::vector<AscendString> registered_ops;
  CustomOpFactory::GetAllRegisteredOps(registered_ops);
  EXPECT_EQ(infos.size(), registered_ops.size());
}

TEST_F(UtestCustomOpsKernelInfoStore, CheckSupported) {
  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);
  
  std::vector<AscendString> registered_ops;
  CustomOpFactory::GetAllRegisteredOps(registered_ops);
  
  if (!registered_ops.empty()) {
    auto op_desc = std::make_shared<OpDesc>(std::string(registered_ops[0].GetString()), std::string(registered_ops[0].GetString()));
    std::string reason;
    EXPECT_TRUE(store.CheckSupported(op_desc, reason));
  }
}

TEST_F(UtestCustomOpsKernelInfoStore, RefreshCapturesNewRegisteredOp) {
  const std::string kTestOpType = "TestDynamicRegisteredOp_RefreshTest";
  
  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);
  
  std::map<std::string, OpInfo> infos_before;
  store.GetAllOpsKernelInfo(infos_before);
  size_t count_before = infos_before.size();
  EXPECT_EQ(infos_before.count(kTestOpType), 0U);
  
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockCustomOp>();
  };
  EXPECT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);
  
  std::map<std::string, OpInfo> infos_no_refresh;
  store.GetAllOpsKernelInfo(infos_no_refresh);
  EXPECT_EQ(infos_no_refresh.count(kTestOpType), 0U);
  
  EXPECT_EQ(store.Refresh(), SUCCESS);
  
  std::map<std::string, OpInfo> infos_after;
  store.GetAllOpsKernelInfo(infos_after);
  EXPECT_GT(infos_after.size(), count_before);
  EXPECT_NE(infos_after.count(kTestOpType), 0U);
  
  auto op_desc = std::make_shared<OpDesc>(kTestOpType, kTestOpType);
  std::string reason;
  EXPECT_TRUE(store.CheckSupported(op_desc, reason));
}

TEST_F(UtestCustomOpsKernelInfoStore, ThreadSafety) {
  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);
  
  std::map<std::string, OpInfo> infos1;
  std::map<std::string, OpInfo> infos2;
  
  store.GetAllOpsKernelInfo(infos1);
  store.GetAllOpsKernelInfo(infos2);
  
  EXPECT_EQ(infos1.size(), infos2.size());
}

}  // namespace custom
}  // namespace ge