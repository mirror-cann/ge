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

#include <vector>

#include "common/share_graph.h"
#include "framework/common/helper/model_helper.h"
#include "framework/common/framework_types_internal.h"
#include "stub/gert_runtime_stub.h"

namespace ge {
namespace {

class UtestModelHelperMisc : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(UtestModelHelperMisc, CheckOsCpuInfoAndOppVersion_Success) {
  ModelHelper model_helper;
  std::vector<char> data(256);
  ModelFileHeader *file_header = reinterpret_cast<ModelFileHeader *>(data.data());
  file_header->need_check_os_cpu_info = static_cast<uint8_t>(OsCpuInfoCheckTyep::NO_CHECK);
  model_helper.file_header_ = file_header;
  model_helper.is_unknown_shape_model_ = true;
  gert::GertRuntimeStub stub;
  stub.GetSlogStub().Clear();
  stub.GetSlogStub().SetLevelDebug();
  EXPECT_EQ(model_helper.CheckOsCpuInfoAndOppVersion(), SUCCESS);
  ASSERT_TRUE(stub.GetSlogStub().FindLog(DLOG_DEBUG, "Check opp version[] success") >= 0);
}

TEST_F(UtestModelHelperMisc, UpdateSessionGraphId) {
  ModelHelper model_helper;
  bool refreshed = false;
  auto graph = gert::ShareGraph::BuildWithKnownSubgraphWithTwoConst();
  auto ret = model_helper.UpdateSessionGraphId(graph, "1", refreshed);
  EXPECT_EQ(ret, SUCCESS);
}

}  // namespace
}  // namespace ge
