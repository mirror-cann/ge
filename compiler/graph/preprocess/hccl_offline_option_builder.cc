/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_offline_option_builder.h"
#include <fstream>
#include "graph/utils/file_utils.h"
#include "common/checker.h"
#include "ge/ge_api_error_codes.h"
#include "nlohmann/json.hpp"

namespace ge {

HcclOfflineOptionBuilder &HcclOfflineOptionBuilder::Instance() {
  static HcclOfflineOptionBuilder hccl_offline_option_builder;
  return hccl_offline_option_builder;
}

Status HcclOfflineOptionBuilder::Initialize(const std::string &soc_version,
                                            const std::string &logic_topo_config,
                                            const std::string &hccl_sub_comm_config) {
  if (inited_) {
    GELOGI("Hccl offline option builder has already been initialized. Skip initial again.");
    return SUCCESS;
  }

  logic_topo_config_path_ = RealPath(logic_topo_config.c_str());
  hccl_sub_comm_config_path_ = RealPath(hccl_sub_comm_config.c_str());
  soc_version_ = soc_version;
  if ((logic_topo_config_path_.empty()) || (soc_version_.empty()) || (hccl_sub_comm_config_path_.empty())) {
    GELOGW("Current offline model should neither contain hcom ops nor enable algorithm to split model "
           "result of empty logic_topo_path(%s) or soc_version(%s) or hccl_sub_comm_config(%s).",
           logic_topo_config_path_.c_str(), soc_version_.c_str(), hccl_sub_comm_config_path_.c_str());
    return SUCCESS;
  }
  GE_ASSERT_SUCCESS(ParseLogicNumaConfig(), "Parse Logic NumaConfig failed");
  GE_ASSERT_SUCCESS(ParseHcclSubCommConfig(), "Parse Hccl Sub Comm Config failed");
  inited_ = true;
  return SUCCESS;
}

void HcclOfflineOptionBuilder::Finalize() {
  logic_rank_table_.clear();
  soc_version_.clear();
  logic_topo_config_path_.clear();
  hccl_sub_comm_config_path_.clear();
  hccl_comm_config_.clear();
  hcomm_grouplist_.clear();
  inited_ = false;
}

Status HcclOfflineOptionBuilder::ParseLogicNumaConfig() {
  GELOGI("Start parser cluster config from file: %s", logic_topo_config_path_.c_str());
  std::ifstream fin(logic_topo_config_path_);
  if (!fin.is_open()) {
    GELOGE(FAILED, "Read cluster config %s failed", logic_topo_config_path_.c_str());
    return FAILED;
  }
  try {
    nlohmann::json js;
    fin >> js;
    auto json_obj = js.find("RankTable");
    GE_ASSERT_TRUE(json_obj != js.end(), "RankTable is missing in JSON config file.");
    logic_rank_table_ = json_obj->dump();
    json_obj = js.find("HcclCommConfig");
    GE_ASSERT_TRUE(json_obj != js.end(), "HcclCommConfig is missing in JSON config file.");
    hccl_comm_config_ = json_obj->dump();
  } catch (const nlohmann::json::exception &e) {
    GELOGE(FAILED, "Parser json file %s failed. %s", logic_topo_config_path_.c_str(), e.what());
    return FAILED;
  }
  return SUCCESS;
}

Status HcclOfflineOptionBuilder::ParseHcclSubCommConfig() {
  GELOGI("Start parser cluster config from file: %s", hccl_sub_comm_config_path_.c_str());
  std::ifstream fin(hccl_sub_comm_config_path_);
  if (!fin.is_open()) {
    GELOGE(FAILED, "Read cluster config %s failed", hccl_sub_comm_config_path_.c_str());
    return FAILED;
  }
  try {
    nlohmann::json js;
    fin >> js;
    hcomm_grouplist_ = js.dump();
    GELOGI("Get cluster config %s success.", hcomm_grouplist_.c_str());
  } catch (const nlohmann::json::exception &e) {
    GELOGE(FAILED, "Parser json file %s failed. %s", hccl_sub_comm_config_path_.c_str(), e.what());
    return FAILED;
  }
  return SUCCESS;
}
} // namespace ge
