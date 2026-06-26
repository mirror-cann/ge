/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "engines/custom_engine/custom_ops_kernel_info_store.h"
#include <memory>
#include <mutex>
#include "framework/common/ge_inner_error_codes.h"
#include "framework/common/debug/ge_log.h"
#include "common/ge_common/ge_types.h"
#include "common/checker.h"
#include "graph/custom_op_factory.h"
#include "graph/ascend_string.h"

namespace ge {
namespace custom {
Status CustomOpsKernelInfoStore::UpdateOpInfoMap() {
  OpInfo default_op_info = {.engine = kEngineNameCustom,
                            .opKernelLib = kCustomOpKernelLibName,
                            .computeCost = 0,
                            .flagPartial = false,
                            .flagAsync = false,
                            .isAtomic = false,
                            .opFileName = "",
                            .opFuncName = ""};
  std::vector<AscendString> all_ops_types;
  GE_ASSERT_SUCCESS(CustomOpFactory::GetAllRegisteredOps(all_ops_types));
  {
    std::lock_guard<std::mutex> lock(mu_);
    op_info_map_.clear();
    for (auto &op_type : all_ops_types) {
      op_info_map_[std::string(op_type.GetString())] = default_op_info;
    }
  }
  return SUCCESS;
}

Status CustomOpsKernelInfoStore::Initialize(const std::map<std::string, std::string> &options) {
  (void)options;
  GE_ASSERT_SUCCESS(UpdateOpInfoMap());
  GELOGI("CustomOpsKernelInfoStore initialized, op info num: %zu", op_info_map_.size());
  return SUCCESS;
}

Status CustomOpsKernelInfoStore::Finalize() {
  std::lock_guard<std::mutex> lock(mu_);
  op_info_map_.clear();
  return SUCCESS;
}

Status CustomOpsKernelInfoStore::Refresh() {
  GE_ASSERT_SUCCESS(UpdateOpInfoMap());
  GELOGI("CustomOpsKernelInfoStore refreshed, op info num: %zu", op_info_map_.size());
  return SUCCESS;
}

void CustomOpsKernelInfoStore::GetAllOpsKernelInfo(std::map<std::string, OpInfo> &infos) const {
  std::lock_guard<std::mutex> lock(mu_);
  infos = op_info_map_;
}

bool CustomOpsKernelInfoStore::CheckSupported(const OpDescPtr &op_desc, std::string &reason) const {
  (void)reason;
  GE_ASSERT_NOTNULL(op_desc);
  std::lock_guard<std::mutex> lock(mu_);
  return op_info_map_.count(op_desc->GetType()) > 0;
}
}  // namespace custom
}  // namespace ge
