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
#undef private
#undef protected

using namespace std;
using namespace testing;
using namespace fe;

class fusion_priority_manager_stest : public testing::Test {
 protected:
  static void SetUpTestCase() {
    Configuration::Instance(fe::AI_CORE_NAME).ascend_ops_path_ =
        GetCodeDir() + "/tests/engines/nn_engine/st/testcase/fusion_config_manager/builtin_config/";
    Configuration::Instance(fe::AI_CORE_NAME)
        .config_str_param_vec_[static_cast<size_t>(CONFIG_STR_PARAM::FusionLicense)] = "ALL";
    Configuration::Instance(fe::AI_CORE_NAME).content_map_["fusion.config.built-in.file"] = "fusion_config1.json";

    Configuration::Instance(fe::VECTOR_CORE_NAME).ascend_ops_path_ =
        GetCodeDir() + "/tests/engines/nn_engine/st/testcase/fusion_config_manager/builtin_config/";
    Configuration::Instance(fe::VECTOR_CORE_NAME)
        .config_str_param_vec_[static_cast<size_t>(CONFIG_STR_PARAM::FusionLicense)] = "ALL";
    Configuration::Instance(fe::VECTOR_CORE_NAME).content_map_["fusion.config.built-in.file"] = "fusion_config1.json";
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

TEST_F(fusion_priority_manager_stest, test_ai_core_engine_initialize_success) {
  FEOpsKernelInfoStorePtr ops_kernel_info_store_ptr = std::make_shared<FEOpsKernelInfoStore>(fe::AI_CORE_NAME);
  std::map<std::string, std::string> options;
  ops_kernel_info_store_ptr->Initialize(options);
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(ops_kernel_info_store_ptr);

  FusionPriorityManager manager(fe::AI_CORE_NAME, fusion_rule_mgr_ptr);
  Status status = manager.Initialize();
  EXPECT_EQ(status, fe::SUCCESS);

  vector<FusionPassOrRule> custom_pass_or_rule_vec;
  Status status2 = manager.InitCustomPasses(false, custom_pass_or_rule_vec);
  EXPECT_EQ(status2, fe::SUCCESS);
}

TEST_F(fusion_priority_manager_stest, test_vector_core_engine_coverage244) {
  FEOpsKernelInfoStorePtr ops_kernel_info_store_ptr = std::make_shared<FEOpsKernelInfoStore>(fe::VECTOR_CORE_NAME);
  std::map<std::string, std::string> options;
  ops_kernel_info_store_ptr->Initialize(options);
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(ops_kernel_info_store_ptr);

  FusionPriorityManager manager(fe::VECTOR_CORE_NAME, fusion_rule_mgr_ptr);
  Status status = manager.Initialize();
  EXPECT_EQ(status, fe::SUCCESS);

  vector<FusionPassOrRule> custom_pass_or_rule_vec;
  Status status2 = manager.InitCustomPasses(false, custom_pass_or_rule_vec);
  EXPECT_EQ(status2, fe::SUCCESS);
}

TEST_F(fusion_priority_manager_stest, test_invalid_engine_coverage246to249) {
  FEOpsKernelInfoStorePtr ops_kernel_info_store_ptr = std::make_shared<FEOpsKernelInfoStore>(fe::AI_CORE_NAME);
  std::map<std::string, std::string> options;
  ops_kernel_info_store_ptr->Initialize(options);
  FusionRuleManagerPtr fusion_rule_mgr_ptr = std::make_shared<FusionRuleManager>(ops_kernel_info_store_ptr);

  FusionPriorityManager manager("INVALID_ENGINE", fusion_rule_mgr_ptr);

  vector<FusionPassOrRule> custom_pass_or_rule_vec;
  Status status = manager.InitCustomPasses(false, custom_pass_or_rule_vec);
  EXPECT_EQ(status, fe::FAILED);
}

TEST_F(fusion_priority_manager_stest, test_get_fusion_passes_and_rules_coverage311to320) {
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

TEST_F(fusion_priority_manager_stest, test_init_custom_rules_failed_coverage262to266) {
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

TEST_F(fusion_priority_manager_stest, test_get_fusion_passes_and_rules_init_custom_rules_failed_coverage313) {
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

TEST_F(fusion_priority_manager_stest, test_get_graph_fusion_rule_infos_by_type_invalid_rule_type) {
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

TEST_F(fusion_priority_manager_stest, test_init_built_in_rules_failed) {
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
