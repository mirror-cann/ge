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
#include "common/aicore_util_constants.h"
#define protected public
#define private public
#include "graph/ge_local_context.h"
#include "common/configuration.h"
#include "fusion_config_manager/fusion_priority_manager.h"
#include "fusion_config_manager/fusion_config_parser.h"
#include "register/graph_optimizer/graph_fusion/fusion_pass_manager/fusion_pass_registry.h"
#undef private
#undef protected

using namespace std;
using namespace testing;
using namespace fe;

class UTestFusionPriorityManagerPrecise : public testing::Test {
 protected:
  static void SetUpTestCase() {
    Configuration::Instance(fe::AI_CORE_NAME).ascend_ops_path_ =
        GetCodeDir() + "/tests/engines/nn_engine/ut/testcase/fusion_engine/fusion_config_manager/builtin_config/";
    Configuration::Instance(fe::VECTOR_CORE_NAME).ascend_ops_path_ =
        GetCodeDir() + "/tests/engines/nn_engine/ut/testcase/fusion_engine/fusion_config_manager/builtin_config/";
  }
  static void TearDownTestCase() {
    Configuration::Instance(fe::AI_CORE_NAME).InitLibPath();
    Configuration::Instance(fe::AI_CORE_NAME).InitAscendOpsPath();
    Configuration::Instance(fe::VECTOR_CORE_NAME).InitLibPath();
    Configuration::Instance(fe::VECTOR_CORE_NAME).InitAscendOpsPath();
  }
  void SetUp() {}
  void TearDown() {}
};

// 覆盖242-248行：VECTOR_CORE引擎分支
TEST_F(UTestFusionPriorityManagerPrecise, TestInitCustomPasses_VectorCore_Coverage242to248) {
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(fe::VECTOR_CORE_NAME);
  FusionPriorityManager manager(fe::VECTOR_CORE_NAME, fusion_rule_mgr_ptr);
  manager.Initialize();

  vector<FusionPassOrRule> custom_pass_or_rule_vec;
  Status status = manager.InitCustomPasses(false, custom_pass_or_rule_vec);
  EXPECT_EQ(status, SUCCESS);
}

// 覆盖245-248行：无效引擎名称
TEST_F(UTestFusionPriorityManagerPrecise, TestInitCustomPasses_InvalidEngine_Coverage245to248) {
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>("InvalidEngine");
  FusionPriorityManager manager("InvalidEngine", fusion_rule_mgr_ptr);

  vector<FusionPassOrRule> custom_pass_or_rule_vec;
  Status status = manager.InitCustomPasses(false, custom_pass_or_rule_vec);
  EXPECT_EQ(status, FAILED);
}

// 覆盖310, 316, 319行：GetGraphFusionPassesAndRules完整流程
TEST_F(UTestFusionPriorityManagerPrecise, TestGetGraphFusionPassesAndRules_Complete_Coverage310_316_319) {
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(fe::AI_CORE_NAME);
  FusionPriorityManager manager(fe::AI_CORE_NAME, fusion_rule_mgr_ptr);
  manager.Initialize();

  vector<FusionPassOrRule> custom_pass_or_rule_vec;
  vector<FusionPassOrRule> built_in_pass_or_rule_vec;
  Status status = manager.GetGraphFusionPassesAndRules(false, custom_pass_or_rule_vec, built_in_pass_or_rule_vec);
  EXPECT_EQ(status, SUCCESS);
}

// 覆盖278-280行：InitBuiltInPasses VECTOR_CORE分支
TEST_F(UTestFusionPriorityManagerPrecise, TestInitBuiltInPasses_VectorCore_Coverage278to280) {
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(fe::VECTOR_CORE_NAME);
  FusionPriorityManager manager(fe::VECTOR_CORE_NAME, fusion_rule_mgr_ptr);
  manager.Initialize();

  vector<FusionPassOrRule> built_in_pass_or_rule_vec;
  Status status = manager.InitBuiltInPasses(false, built_in_pass_or_rule_vec);
  EXPECT_EQ(status, SUCCESS);
}

// 覆盖SortBufferFusion VECTOR_CORE场景
TEST_F(UTestFusionPriorityManagerPrecise, TestSortBufferFusion_VectorCore) {
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(fe::VECTOR_CORE_NAME);
  FusionPriorityManager manager(fe::VECTOR_CORE_NAME, fusion_rule_mgr_ptr);
  manager.Initialize();

  Status status = manager.SortBufferFusion();
  EXPECT_EQ(status, SUCCESS);
}

// 覆盖SortGraphFusion场景
TEST_F(UTestFusionPriorityManagerPrecise, TestSortGraphFusion_Complete) {
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(fe::AI_CORE_NAME);
  FusionPriorityManager manager(fe::AI_CORE_NAME, fusion_rule_mgr_ptr);
  manager.Initialize();

  Status status = manager.SortGraphFusion();
  EXPECT_EQ(status, SUCCESS);
}
