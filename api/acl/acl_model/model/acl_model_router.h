/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ASCEND_ACL_MODEL_ROUTER_H
#define ASCEND_ACL_MODEL_ROUTER_H

#include <cstddef>
#include <cstdint>

#include "acl/acl_mdl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 通过 modelDesc 判断是否为 OM2 模型
 * @param modelDesc 模型描述符指针
 * @param isOm2 输出参数，true 表示是 OM2 模型，false 表示不是
 * @return aclError 错误码
 */
ACL_FUNC_VISIBILITY aclError AclIsOm2ModelByDesc(const aclmdlDesc *modelDesc, bool *isOm2);

/**
 * @brief 通过模型 ID 判断是否为 OM2 模型
 * @param modelId 模型 ID
 * @param isOm2 输出参数，true 表示是 OM2 模型，false 表示不是
 * @return aclError 错误码
 */
ACL_FUNC_VISIBILITY aclError AclIsOm2ModelById(uint32_t modelId, bool *isOm2);

/**
 * @brief 通过文件路径判断是否为 OM2 模型
 * @param modelPath 模型文件路径
 * @param isOm2 输出参数，true 表示是 OM2 模型，false 表示不是
 * @return aclError 错误码
 */
ACL_FUNC_VISIBILITY aclError AclIsOm2ModelByPath(const char *modelPath, bool *isOm2);

/**
 * @brief 通过内存数据判断是否为 OM2 模型
 * @param modelData 模型数据指针
 * @param modelSize 模型数据大小
 * @param isOm2 输出参数，true 表示是 OM2 模型，false 表示不是
 * @return aclError 错误码
 */
ACL_FUNC_VISIBILITY aclError AclIsOm2ModelByData(const void *modelData, size_t modelSize, bool *isOm2);

/**
 * @brief 通过配置句柄判断是否为 OM2 模型
 * @param handle 模型配置句柄
 * @param isOm2 输出参数，true 表示是 OM2 模型，false 表示不是
 * @return aclError 错误码
 */
ACL_FUNC_VISIBILITY aclError AclIsOm2ModelByConfig(const aclmdlConfigHandle *handle, bool *isOm2);

/**
 * @brief 通过 Bundle ID 判断是否为 OM2 Bundle
 * @param bundleId Bundle ID
 * @param isOm2 输出参数，true 表示是 OM2 Bundle，false 表示不是
 * @return aclError 错误码
 */
ACL_FUNC_VISIBILITY aclError AclIsOm2BundleById(uint32_t bundleId, bool *isOm2);

#ifdef __cplusplus
}
#endif

#endif  // ASCEND_ACL_MODEL_ROUTER_H
