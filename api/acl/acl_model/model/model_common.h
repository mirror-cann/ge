/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACL_MODEL_SRC_MODEL_MODEL_COMMON_H_
#define ACL_MODEL_SRC_MODEL_MODEL_COMMON_H_

#include <vector>
#include <memory>
#include <string>
#include <set>
#include "acl/acl_base.h"
#include "acl/acl_mdl.h"
#include "framework/runtime/rt_session.h"

// Common enums for tensor/dims operations
enum class TensorType : std::uint8_t {
    INPUT_TENSOR_TYPE = 0,
    OUTPUT_TENSOR_TYPE
};

enum class DimsType : std::uint8_t {
    DIMS_TYPE_V1 = 0,
    DIMS_TYPE_V2
};

namespace acl {

// Bundle 子模型信息
struct BundleSubModelInfo {
  size_t workSize = 0U;
  size_t weightSize = 0U;
  size_t offset = 0U;
  size_t modelSize = 0U;
};

// Bundle 模型信息
struct BundleModelInfo {
  bool isInit = false; // aclmdlBundleInit scene means true
  std::shared_ptr<gert::RtSession> rtSession;
  size_t varSize = 0U;
  std::string fromFilePath;
  std::shared_ptr<const uint8_t> bundleModelData;
  size_t bundleModelSize = 0U;
  std::vector<BundleSubModelInfo> subModelInfos;
  std::vector<uint32_t> loadedSubModelId; // aclmdlBundleGetModelId use this when aclmdlBundleLoadFromxx is called
  std::set<uint32_t> loadedSubModelIdSet;
};

} // namespace acl


/**
 * @brief Get dims from model description
 * @param modelDesc Model description
 * @param tensorType Tensor type (input/output)
 * @param dimsType Dims type (v1/v2)
 * @param idx Index
 * @param dims Output dims
 * @return ACL_SUCCESS on success, error code otherwise
 */
aclError GetDimsFromModelDesc(const aclmdlDesc *modelDesc, TensorType tensorType,
                              DimsType dimsType, size_t idx, aclmdlIODims *dims);

/**
 * @brief Check if user load config options are valid for OM2
 * @param handle Config handle
 * @return ACL_SUCCESS on success, error code otherwise
 */
aclError CheckOm2UserLoadConfigOptValid(const aclmdlConfigHandle *handle);

#endif // ACL_MODEL_SRC_MODEL_MODEL_COMMON_H_
