/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_OFFLINE_OPTION_BUILDER_H
#define HCCL_OFFLINE_OPTION_BUILDER_H
#include <string>
#include "ge/ge_api_error_codes.h"

namespace ge {
class HcclOfflineOptionBuilder {
 public:
  static HcclOfflineOptionBuilder &Instance();
  Status Initialize(const std::string &soc_version, const std::string &logic_topo_config, const std::string &hccl_sub_comm_config);
  void Finalize();
  const std::string &GetLogicRankTable() const { return logic_rank_table_; }
  const std::string &GetHcclCommConfig() const { return hccl_comm_config_; }
  const std::string &GetHcomGrouplist() const { return hcomm_grouplist_; }
  const std::string &GetSocVersion() const { return soc_version_; }
  bool IsInitialized() const { return inited_; }
 private:
  HcclOfflineOptionBuilder() = default;
  ~HcclOfflineOptionBuilder() = default;
  Status ParseLogicNumaConfig();
  Status ParseHcclSubCommConfig();

  bool inited_ = false;
  std::string logic_rank_table_;
  std::string soc_version_;
  std::string logic_topo_config_path_;
  std::string hccl_sub_comm_config_path_;
  std::string hccl_comm_config_;
  std::string hcomm_grouplist_;
};
}
#endif // HCCL_OFFLINE_OPTION_BUILDER_H
