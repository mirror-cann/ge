/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <functional>
#include <memory>
#include <hccl/hccl_types.h>
#define private public
#include "graph/load/model_manager/task_info/hccl/hccl_util.h"
#undef private
#include "graph/manager/graph_manager.h"
#include "graph/preprocess/hccl_offline_option_builder.h"

namespace ge {
namespace {
bool WriteTextFile(const std::string &file_path, const std::string &content) {
  std::ofstream ofs(file_path, std::ios::out | std::ios::trunc);
  if (!ofs.is_open()) {
    return false;
  }
  ofs << content;
  return ofs.good();
}
}

class UtestHccl: public testing::Test {
 protected:
  void SetUp() override {
    graph_options_backup_ = GetThreadLocalContext().GetAllGraphOptions();
    HcclOfflineOptionBuilder::Instance().Finalize();
  }
  void TearDown() override {
    HcclOfflineOptionBuilder::Instance().Finalize();
    GetThreadLocalContext().SetGraphOption(graph_options_backup_);
  }

  std::map<std::string, std::string> graph_options_backup_;
};

TEST_F(UtestHccl, set_default_hccl_options_from_offline_builder) {
  const std::string logic_topo_config = "./hccl_offline_option_builder_logic_st.json";
  const std::string hccl_sub_comm_config = "./hccl_offline_option_builder_sub_st.json";
  ASSERT_TRUE(WriteTextFile(logic_topo_config,
                            R"({"RankTable":[{"rank_id":"1"}],"HcclCommConfig":{"graph_mode":"1"}})"));
  ASSERT_TRUE(WriteTextFile(hccl_sub_comm_config,
                            R"({"group_list":[{"group_name":"g1","group_rank_list":[1,2]}]})"));

  auto &builder = HcclOfflineOptionBuilder::Instance();
  ASSERT_EQ(builder.Initialize("Ascend910B2", logic_topo_config, hccl_sub_comm_config), SUCCESS);
  ASSERT_TRUE(builder.IsInitialized());

  GraphManager graph_manager;
  GetThreadLocalContext().SetGraphOption({});
  EXPECT_EQ(graph_manager.SetDefaultHcclOptions(), SUCCESS);
  auto graph_options = GetThreadLocalContext().GetAllGraphOptions();
  EXPECT_EQ(graph_options[OPTION_EXEC_RANK_TABLE], R"([{"rank_id":"1"}])");
  EXPECT_EQ(graph_options[OPTION_EXEC_GLOBAL_HCCL_COMM_CONFIG], R"({"graph_mode":"1"})");
  EXPECT_EQ(graph_options[OPTION_EXEC_HCOM_GROUPLIST_V2],
            R"({"group_list":[{"group_name":"g1","group_rank_list":[1,2]}]})");
  EXPECT_EQ(graph_options[OPTION_HCCL_COMPILER_OFFLINE], "1");
  EXPECT_EQ(graph_options[SOC_VERSION], "Ascend910B2");

  GetThreadLocalContext().SetGraphOption({{SOC_VERSION, "ManualSoc"}});
  EXPECT_EQ(graph_manager.SetDefaultHcclOptions(), SUCCESS);
  graph_options = GetThreadLocalContext().GetAllGraphOptions();
  EXPECT_EQ(graph_options[SOC_VERSION], "ManualSoc");
  EXPECT_EQ(graph_options[OPTION_EXEC_HCOM_GROUPLIST_V2],
            R"({"group_list":[{"group_name":"g1","group_rank_list":[1,2]}]})");

  (void)std::remove(logic_topo_config.c_str());
  (void)std::remove(hccl_sub_comm_config.c_str());
}

}
