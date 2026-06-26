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
#define protected public
#define private public
#include "rts_ops_kernel_builder.h"
#include "rts_ffts_plus_ops_kernel_builder.h"
#include "graph/compute_graph.h"
#include "rts_engine_utest_stub.h"
#include "rt_external_model.h"
#include "common/util.h"
#undef protected
#undef private
using namespace testing;
using namespace ge;
using std::string;

namespace cce {
namespace runtime {

class RtsOpsKernelBuilderTest : public testing::Test {
 protected:
  static void SetUpTestCase() {
    std::cout << "RtsOpsKernelBuilderTest SetUPTestCase" << std::endl;
  }

  static void TearDownTestCase() {
    std::cout << "RtsOpsKernelBuilderTest Tear Down" << std::endl;
  }

  virtual void SetUp() {
    std::cout << "RtsOpsKernelBuilderTest SetUP" << std::endl;
  }

  virtual void TearDown() {}
};

TEST_F(RtsOpsKernelBuilderTest, Initialize) {
  RtsOpsKernelBuilder opsKernelBuilder;
  map<string, string> options;
  ge::Status ret = opsKernelBuilder.Initialize(options);
  ASSERT_EQ(ret, ge::SUCCESS);
}

TEST_F(RtsOpsKernelBuilderTest, TestFftsPlusInitialize) {
  RtsFftsPlusOpsKernelBuilder opsKernelBuilder;
  map<string, string> options;
  ge::Status ret = opsKernelBuilder.Initialize(options);
}

TEST_F(RtsOpsKernelBuilderTest, TestFftsPlusFinalize) {
  RtsFftsPlusOpsKernelBuilder opsKernelBuilder;
  ge::Status ret = opsKernelBuilder.Finalize();
  ASSERT_EQ(ret, ge::SUCCESS);
}

}  // namespace runtime
}  // namespace cce
