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

#include "fe_llt_utils.h"
#define protected public
#define private public
#include "fusion_manager/fusion_manager.h"
#include "itf_handler/itf_handler.h"
#include "common/configuration.h"
#undef private
#undef protected

using namespace std;
using namespace fe;
using namespace ge;

class itfhandler_unittest : public testing::Test {
 protected:
  static void TearDownTestCase() {}
  // AUTO GEN PLEASE DO NOT MODIFY IT
};

TEST_F(itfhandler_unittest, initialize_and_finalize) {
  Status ret = Finalize();
  EXPECT_EQ(ret, fe::SUCCESS);
  map<string, string> options;
  options.emplace("ge.socVersion", "Ascend910B1");
  Configuration::Instance(AI_CORE_NAME).is_init_ = false;
  Configuration::Instance(AI_CORE_NAME).lib_path_ =
      GetCodeDir() + "/tests/engines/nn_engine/depends/CANN_910b_stub/cann/x86_64-linux/lib64/";
  Configuration::Instance(AI_CORE_NAME).ascend_ops_path_ = GetCodeDir();
  ret = Initialize(options);
  EXPECT_NE(ret, fe::SUCCESS);
  string stub_cann_path = fe::GetCodeDir() + "/tests/engines/nn_engine/depends/CANN_910b_stub/cann";
  fe::EnvVarGuard cann_guard(MM_ENV_ASCEND_HOME_PATH, stub_cann_path.c_str());
  string stub_opp_path = fe::GetCodeDir() + "/tests/engines/nn_engine/depends/CANN_910b_stub/cann/opp";
  fe::EnvVarGuard opp_guard(MM_ENV_ASCEND_OPP_PATH, stub_opp_path.c_str());
  EXPECT_EQ(fe::InitPlatformInfo("Ascend910B1", true), 0);
  options["ge.bufferOptimize"] = "lx_optimize";
  Configuration::Instance(AI_CORE_NAME).is_init_ = false;
  Configuration::Instance(AI_CORE_NAME).lib_path_ =
      GetCodeDir() + "/tests/engines/nn_engine/depends/CANN_910b_stub/cann/x86_64-linux/lib64/";
  Configuration::Instance(AI_CORE_NAME).ascend_ops_path_ = GetCodeDir();
  ret = Initialize(options);
  options["ge.bufferOptimize"] = "l2_optimize";
  Configuration::Instance(AI_CORE_NAME).is_init_ = false;
  Configuration::Instance(AI_CORE_NAME).lib_path_ =
      GetCodeDir() + "/tests/engines/nn_engine/depends/CANN_910b_stub/cann/x86_64-linux/lib64/";
  Configuration::Instance(AI_CORE_NAME).ascend_ops_path_ = GetCodeDir();
  ret = Initialize(options);
  cann_guard.Restore();
  opp_guard.Restore();
}

TEST_F(itfhandler_unittest, GetOpsKernelInfoStores_suc) {
  FusionManager &fm_ai = FusionManager::Instance(AI_CORE_NAME);
  fm_ai.ops_kernel_info_store_ = make_shared<FEOpsKernelInfoStore>(AI_CORE_NAME);

  FusionManager &fm_vec = FusionManager::Instance(VECTOR_CORE_NAME);
  fm_vec.ops_kernel_info_store_ = make_shared<FEOpsKernelInfoStore>(VECTOR_CORE_NAME);

  map<string, OpsKernelInfoStorePtr> op_kern_infos;
  fm_ai.GetOpsKernelInfoStores(op_kern_infos, AI_CORE_NAME);
  fm_vec.GetOpsKernelInfoStores(op_kern_infos, VECTOR_CORE_NAME);
  EXPECT_EQ(op_kern_infos.size(), 2);
}

TEST_F(itfhandler_unittest, get_graph_optimizer_objs_success) {
  FusionManager &fm_ai = FusionManager::Instance(AI_CORE_NAME);
  fm_ai.ops_kernel_info_store_ = make_shared<FEOpsKernelInfoStore>(AI_CORE_NAME);
  fm_ai.graph_opt_ = make_shared<FEGraphOptimizer>(fm_ai.ops_kernel_info_store_, AI_CORE_NAME);

  FusionManager &fm_vec = FusionManager::Instance(VECTOR_CORE_NAME);
  fm_vec.ops_kernel_info_store_ = make_shared<FEOpsKernelInfoStore>(VECTOR_CORE_NAME);
  fm_vec.graph_opt_ = make_shared<FEGraphOptimizer>(fm_vec.ops_kernel_info_store_, VECTOR_CORE_NAME);

  map<string, GraphOptimizerPtr> graph_optimizers;
  fm_ai.GetGraphOptimizerObjs(graph_optimizers, AI_CORE_NAME);
  fm_vec.GetGraphOptimizerObjs(graph_optimizers, VECTOR_CORE_NAME);
  EXPECT_EQ(graph_optimizers.size(), 2);
}
