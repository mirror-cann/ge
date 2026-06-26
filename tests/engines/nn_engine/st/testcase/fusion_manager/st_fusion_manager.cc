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
#include <nlohmann/json.hpp>
#include <iostream>
#include <list>
#define protected public
#define private public
#include "platform/platform_info.h"
#include "common/platform_utils.h"
#undef private
#undef protected
#include "fusion_manager/fusion_manager.h"
#include "fe_llt_utils.h"

using namespace std;
using namespace fe;

class fuison_manager_stest : public testing::Test {
 protected:
  static void SetUpTestCase() {}
};

TEST_F(fuison_manager_stest, dsa_instance) {
  FusionManager &fm = FusionManager::Instance(kDsaCoreName);
  fm.ops_kernel_info_store_ = make_shared<FEOpsKernelInfoStore>(fe::kDsaCoreName);
  fm.dsa_graph_opt_ = make_shared<DSAGraphOptimizer>(fm.ops_kernel_info_store_, fe::kDsaCoreName);
  map<string, GraphOptimizerPtr> graph_optimizers;
  std::string AIcoreEngine = "DSAEngine";
  fm.GetGraphOptimizerObjs(graph_optimizers, AIcoreEngine);
  EXPECT_EQ(graph_optimizers.size(), 1);
}
