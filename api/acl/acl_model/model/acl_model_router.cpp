/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "acl_resource_manager_om2.h"
#include "acl_model_router.h"

#include "error_codes_inner.h"
#include "acl/acl_mdl.h"
#include "model_desc_internal.h"
#include "framework/runtime/om2_model_executor.h"
#include "common/log_inner.h"

#ifdef __cplusplus
extern "C" {
#endif

aclError AclIsOm2ModelByDesc(const aclmdlDesc *modelDesc, bool *isOm2) {
  if (modelDesc == nullptr) {
    ACL_LOG_ERROR("modelDesc is nullptr");
    return ACL_ERROR_INVALID_PARAM;
  }
  *isOm2 = acl::AclResourceManagerOm2::GetInstance().IsOm2ModelById(modelDesc->modelId);
  return ACL_ERROR_NONE;
}

aclError AclIsOm2ModelById(uint32_t modelId, bool *isOm2) {
  *isOm2 = acl::AclResourceManagerOm2::GetInstance().IsOm2ModelById(modelId);
  return ACL_ERROR_NONE;
}

aclError AclIsOm2ModelByPath(const char *modelPath, bool *isOm2) {
  if (modelPath == nullptr) {
    ACL_LOG_ERROR("modelPath is nullptr");
    return ACL_ERROR_INVALID_PARAM;
  }
  ge::Status ret = gert::IsOm2Model(modelPath, *isOm2);
  if (ret != ge::SUCCESS) {
    ACL_LOG_ERROR("failed to check if model is OM2 by path, path=%s, result=%u", modelPath, ret);
    return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
  }
  ACL_LOG_DEBUG("model type detected from file path: isOm2Model = %d", *isOm2);
  return ACL_ERROR_NONE;
}

aclError AclIsOm2ModelByData(const void *modelData, size_t modelSize, bool *isOm2) {
  if (modelData == nullptr || modelSize == 0) {
    ACL_LOG_ERROR("modelData is nullptr or modelSize is 0");
    return ACL_ERROR_INVALID_PARAM;
  }
  ge::Status ret = gert::IsOm2Model(modelData, modelSize, *isOm2);
  if (ret != ge::SUCCESS) {
    ACL_LOG_ERROR("failed to check if model is OM2 by data, size=%zu, result=%u", modelSize, ret);
    return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
  }
  ACL_LOG_DEBUG("model type detected from memory data: isOm2Model = %d", *isOm2);
  return ACL_ERROR_NONE;
}

aclError AclIsOm2ModelByConfig(const aclmdlConfigHandle *handle, bool *isOm2) {
  if (handle == nullptr) {
    ACL_LOG_ERROR("config handle is null");
    return ACL_ERROR_INVALID_PARAM;
  }

  ge::Status ret;

  if (!handle->loadPath.empty()) {
    ACL_LOG_DEBUG("detecting model type from file path: %s", handle->loadPath.c_str());
    ret = gert::IsOm2Model(handle->loadPath.c_str(), *isOm2);
    if (ret != ge::SUCCESS) {
      ACL_LOG_ERROR("failed to detect model type from file path, result = %u", ret);
      return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
    }
    ACL_LOG_DEBUG("model type detected from file path: isOm2Model = %d", *isOm2);
    return ACL_ERROR_NONE;
  }

  if (handle->mdlAddr != nullptr && handle->mdlSize > 0) {
    ACL_LOG_DEBUG("detecting model type from memory data, size = %zu", handle->mdlSize);
    ret = gert::IsOm2Model(handle->mdlAddr, handle->mdlSize, *isOm2);
    if (ret != ge::SUCCESS) {
      ACL_LOG_ERROR("failed to detect model type from memory data, result = %u", ret);
      return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
    }
    ACL_LOG_DEBUG("model type detected from memory data: isOm2Model = %d", *isOm2);
    return ACL_ERROR_NONE;
  }

  ACL_LOG_ERROR("config handle has no valid model path or data");
  return ACL_ERROR_INVALID_PARAM;
}

aclError AclIsOm2BundleById(uint32_t bundleId, bool *isOm2) {
  *isOm2 = acl::AclResourceManagerOm2::GetInstance().IsOm2BundleById(bundleId);
  return ACL_ERROR_NONE;
}

#ifdef __cplusplus
}
#endif
