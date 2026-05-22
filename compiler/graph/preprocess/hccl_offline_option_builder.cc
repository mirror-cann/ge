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

  // 如果logic_topo_config与hccl_sub_comm_config均为空，则认为当前离线模型不包含hcom算子，也不需要开启算法切分模型的功能，因此直接返回成功。
  if (logic_topo_config.empty() && hccl_sub_comm_config.empty()) {
    GELOGI("cluster_config and hccl_sub_comm_config are both empty, skip parse config.");
    return SUCCESS;
  }
  // 如果logic_topo_config为空但是hccl_sub_comm_config不为空，则认为当前离线模型包含hcom算子，但是缺少cluster config，需要报错。
  if (logic_topo_config.empty() && !hccl_sub_comm_config.empty()) {
    GELOGE(FAILED, "missing parameter: cluster_config.");
    REPORT_PREDEFINED_ERR_MSG("E10058", std::vector<const char *>({"parameter"}),
                              std::vector<const char *>({"cluster config"}));
    return FAILED;
  }
  // 如果soc_version为空，直接报错，因为soc_version是离线模型中必填的参数。
  if (soc_version.empty()) {
    GELOGE(FAILED, "Soc version is empty.");
    REPORT_PREDEFINED_ERR_MSG("E10058", std::vector<const char *>({"parameter"}),
                              std::vector<const char *>({"soc version"}));
    return FAILED;
  }
  soc_version_ = soc_version;

  // 如果logic_topo_config不为空但是hccl_sub_comm_config为空，则认为当前离线模型包含hcom算子，但是无需配置子通信域信息，此时可以继续进行解析，但需要打印日志告知用户。
  logic_topo_config_path_ = RealPath(logic_topo_config.c_str());
  GE_ASSERT_TRUE(!logic_topo_config_path_.empty(), "cluster config[%s] is invalid", logic_topo_config.c_str());

  if (hccl_sub_comm_config.empty()) {
    GELOGW("Parameter hccl_sub_comm_config is empty.");
  } else {
    hccl_sub_comm_config_path_ = RealPath(hccl_sub_comm_config.c_str());
    GE_ASSERT_TRUE(!hccl_sub_comm_config_path_.empty(), "sub communication config[%s] is invalid", hccl_sub_comm_config.c_str());
  }

  GE_ASSERT_SUCCESS(ParseLogicNumaConfig(), "Parse Logic NumaConfig failed");
  if (!hccl_sub_comm_config_path_.empty()) {
    GE_ASSERT_SUCCESS(ParseHcclSubCommConfig(), "Parse Hccl Sub Comm Config failed");
  }
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
