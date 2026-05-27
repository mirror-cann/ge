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
#include "common/framework_types_internal.h"

#include "macro_utils/dt_public_scope.h"
#include "common/op/ge_op_utils.h"
#include "graph/load/model_manager/model_manager.h"
#include "graph/load/model_manager/aicpu_resources.h"
#include "stub/gert_runtime_stub.h"

using namespace std;
using namespace testing;

namespace ge {

const static std::string ENC_KEY = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

class UtestModelManagerModelManagerAicpu : public testing::Test {
 protected:
  void SetUp() {
    auto mock_acl_runtime = std::make_shared<AclRuntimeStub>();
    ge::AclRuntimeStub::SetInstance(mock_acl_runtime);
  }

  void TearDown() {
    ge::AclRuntimeStub::Reset();
  }
};

TEST_F(UtestModelManagerModelManagerAicpu, checkAicpuOptype) {
  ModelManager model_manager;
  std::vector<std::string> aicpu_op_list;
  std::vector<std::string> aicpu_tf_list;
  aicpu_tf_list.emplace_back("FrameworkOp");
  aicpu_tf_list.emplace_back("Unique");

  EXPECT_EQ(model_manager.LaunchKernelCheckAicpuOp(aicpu_op_list, aicpu_tf_list), SUCCESS);
  // Load allow listener is null
  // EXPECT_EQ(ge::FAILED, mm.LoadModelOffline(model_id, data, nullptr, nullptr));
}

TEST_F(UtestModelManagerModelManagerAicpu, DestroyAicpuKernel) {
  ModelManager model_manager;
  std::vector<std::string> aicpu_op_list;
  std::vector<std::string> aicpu_tf_list;
  aicpu_tf_list.emplace_back("FrameworkOp");
  aicpu_tf_list.emplace_back("Unique");

  EXPECT_EQ(model_manager.DestroyAicpuKernel(0,0,0), SUCCESS);
  // Load allow listener is null
  // EXPECT_EQ(ge::FAILED, mm.LoadModelOffline(model_id, data, nullptr, nullptr));
}

TEST_F(UtestModelManagerModelManagerAicpu, SetStaticModelShapeConfig_AclrtMalloc_Success) {
  AiCpuResources aicpu_resources;
  AiCpuResources::AiCpuModelShapeConfig config;
  config.version = 1;
  config.model_id = 100;
  config.runtime_model_id = 200;
  config.data_len = 64;
  config.data_device_addr = 0x2000;

  std::vector<InputOutputDescInfo> input_desc_list;

  class MockAclRuntime : public ge::AclRuntimeStub {
  public:
    aclError aclrtMalloc(void **devPtr, size_t size, aclrtMemMallocPolicy policy) override {
      static void* mock_addr = reinterpret_cast<void*>(0x3000);
      *devPtr = mock_addr;
      return ACL_SUCCESS;
    }

    aclError aclrtMemcpy(void *dst, size_t dest_max, const void *src, size_t count, aclrtMemcpyKind kind) override {
      if (dst != nullptr && src != nullptr && count > 0) {
        memcpy_s(dst, dest_max, src, count);
      }
      return ACL_SUCCESS;
    }

    aclError aclrtFree(void *devPtr) override {
      return ACL_SUCCESS;
    }
  };
  auto mock_acl_runtime = std::make_shared<MockAclRuntime>();
  ge::AclRuntimeStub::SetInstance(mock_acl_runtime);

  EXPECT_EQ(aicpu_resources.SetStaticModelShapeConfig(config, input_desc_list), SUCCESS);
  ge::AclRuntimeStub::Reset();
}
}  // namespace ge
