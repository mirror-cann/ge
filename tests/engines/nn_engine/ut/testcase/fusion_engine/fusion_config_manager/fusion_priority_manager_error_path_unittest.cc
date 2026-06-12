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
#include "fusion_rule_manager/fusion_rule_manager.h"
#include "ops_kernel_store/fe_ops_kernel_info_store.h"
#include "register/graph_optimizer/graph_fusion/fusion_pass_manager/fusion_pass_registry.h"
#undef private
#undef protected

using namespace std;
using namespace testing;
using namespace fe;

class UTestFusionPriorityManagerErrorPath : public testing::Test {
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

TEST_F(UTestFusionPriorityManagerErrorPath, TestGetGraphFusionPassInfosByType_DefaultBranch) {
  FEOpsKernelInfoStorePtr ops_kernel_info_store_ptr = std::make_shared<FEOpsKernelInfoStore>(fe::AI_CORE_NAME);
  std::map<std::string, std::string> options;
  ops_kernel_info_store_ptr->Initialize(options);
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(ops_kernel_info_store_ptr);
  FusionPriorityManager manager(fe::AI_CORE_NAME, fusion_rule_mgr_ptr);
  manager.Initialize();
  
  vector<FusionPassOrRule> pass_vector;
  GraphFusionPassType invalid_pass_type = static_cast<GraphFusionPassType>(9999);
  
  FusionPassRegistry::GetInstance().RegisterPass(
      invalid_pass_type, 
      "InvalidTestPass",
      []() -> GraphPass* { return nullptr; },
      0);
  
  Status status = manager.GetGraphFusionPassInfosByType(invalid_pass_type, false, pass_vector);
  EXPECT_EQ(status, fe::FAILED);
}

TEST_F(UTestFusionPriorityManagerErrorPath, TestInitCustomRules_GetFusionRulesFailed_Coverage262to266) {
  FEOpsKernelInfoStorePtr ops_kernel_info_store_ptr = std::make_shared<FEOpsKernelInfoStore>(fe::AI_CORE_NAME);
  std::map<std::string, std::string> options;
  ops_kernel_info_store_ptr->Initialize(options);
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(ops_kernel_info_store_ptr);
  
  FusionPriorityManager manager(fe::AI_CORE_NAME, fusion_rule_mgr_ptr);
  manager.Initialize();
  
  fusion_rule_mgr_ptr->init_flag_ = false;
  
  vector<FusionPassOrRule> custom_pass_or_rule_vec;
  Status status = manager.InitCustomRules(custom_pass_or_rule_vec);
  EXPECT_EQ(status, fe::FAILED);
}

TEST_F(UTestFusionPriorityManagerErrorPath, TestGetGraphFusionPassesAndRules_InitCustomRulesFailed_Coverage313) {
  FEOpsKernelInfoStorePtr ops_kernel_info_store_ptr = std::make_shared<FEOpsKernelInfoStore>(fe::AI_CORE_NAME);
  std::map<std::string, std::string> options;
  ops_kernel_info_store_ptr->Initialize(options);
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(ops_kernel_info_store_ptr);
  
  FusionPriorityManager manager(fe::AI_CORE_NAME, fusion_rule_mgr_ptr);
  manager.Initialize();
  
  fusion_rule_mgr_ptr->init_flag_ = false;
  
  vector<FusionPassOrRule> custom_pass_or_rule_vec;
  vector<FusionPassOrRule> built_in_pass_or_rule_vec;
  Status status = manager.GetGraphFusionPassesAndRules(false, custom_pass_or_rule_vec, built_in_pass_or_rule_vec);
  EXPECT_EQ(status, fe::FAILED);
}

TEST_F(UTestFusionPriorityManagerErrorPath, TestGetGraphFusionPassesAndRules_InitBuiltInRulesFailed_Coverage319) {
  FEOpsKernelInfoStorePtr ops_kernel_info_store_ptr = std::make_shared<FEOpsKernelInfoStore>(fe::AI_CORE_NAME);
  std::map<std::string, std::string> options;
  ops_kernel_info_store_ptr->Initialize(options);
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(ops_kernel_info_store_ptr);
  fusion_rule_mgr_ptr->init_flag_ = true;
  
  FusionPriorityManager manager(fe::AI_CORE_NAME, fusion_rule_mgr_ptr);
  manager.Initialize();
  
  vector<FusionPassOrRule> custom_pass_or_rule_vec;
  vector<FusionPassOrRule> built_in_pass_or_rule_vec;
  
  Status status1 = manager.InitCustomPasses(false, custom_pass_or_rule_vec);
  EXPECT_EQ(status1, fe::SUCCESS);
  
  Status status2 = manager.InitCustomRules(custom_pass_or_rule_vec);
  EXPECT_EQ(status2, fe::SUCCESS);
  
  Status status3 = manager.InitBuiltInPasses(false, built_in_pass_or_rule_vec);
  EXPECT_EQ(status3, fe::SUCCESS);
  
  fusion_rule_mgr_ptr->init_flag_ = false;
  
  Status status4 = manager.InitBuiltInRules(built_in_pass_or_rule_vec);
  EXPECT_EQ(status4, fe::FAILED);
}

TEST_F(UTestFusionPriorityManagerErrorPath, TestInitBuiltInRules_GetFusionRulesFailed) {
  FEOpsKernelInfoStorePtr ops_kernel_info_store_ptr = std::make_shared<FEOpsKernelInfoStore>(fe::AI_CORE_NAME);
  std::map<std::string, std::string> options;
  ops_kernel_info_store_ptr->Initialize(options);
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(ops_kernel_info_store_ptr);
  
  FusionPriorityManager manager(fe::AI_CORE_NAME, fusion_rule_mgr_ptr);
  manager.Initialize();
  
  fusion_rule_mgr_ptr->init_flag_ = false;
  
  vector<FusionPassOrRule> built_in_pass_or_rule_vec;
  Status status = manager.InitBuiltInRules(built_in_pass_or_rule_vec);
  EXPECT_EQ(status, fe::FAILED);
}

TEST_F(UTestFusionPriorityManagerErrorPath, TestInitCustomPasses_InvalidEngine) {
  FEOpsKernelInfoStorePtr ops_kernel_info_store_ptr = std::make_shared<FEOpsKernelInfoStore>(fe::AI_CORE_NAME);
  std::map<std::string, std::string> options;
  ops_kernel_info_store_ptr->Initialize(options);
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(ops_kernel_info_store_ptr);
  FusionPriorityManager manager("INVALID_ENGINE", fusion_rule_mgr_ptr);
  
  vector<FusionPassOrRule> custom_pass_or_rule_vec;
  Status status = manager.InitCustomPasses(false, custom_pass_or_rule_vec);
  EXPECT_EQ(status, fe::FAILED);
}

TEST_F(UTestFusionPriorityManagerErrorPath, TestGetGraphFusionRuleInfosByType_InvalidRuleType) {
  FEOpsKernelInfoStorePtr ops_kernel_info_store_ptr = std::make_shared<FEOpsKernelInfoStore>(fe::AI_CORE_NAME);
  std::map<std::string, std::string> options;
  ops_kernel_info_store_ptr->Initialize(options);
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(ops_kernel_info_store_ptr);
  fusion_rule_mgr_ptr->init_flag_ = true;
  
  FusionPriorityManager manager(fe::AI_CORE_NAME, fusion_rule_mgr_ptr);
  manager.Initialize();
  
  vector<FusionPassOrRule> rule_vector;
  RuleType invalid_rule_type = static_cast<RuleType>(9999);
  Status status = manager.GetGraphFusionRuleInfosByType(invalid_rule_type, rule_vector);
  EXPECT_EQ(status, fe::FAILED);
}