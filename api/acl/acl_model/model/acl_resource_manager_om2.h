/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ASCEND_ACL_RESOURCE_MANAGER_OM2_H
#define ASCEND_ACL_RESOURCE_MANAGER_OM2_H

#include <map>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <set>
#include <atomic>
#include "framework/runtime/om2_model_executor.h"
#include "model_common.h"


// C 接口：用于上层调用，避免直接依赖 C++ 类
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 通过模型 ID 判断是否为 OM2 模型
 * @param modelId 模型 ID
 * @return true 表示是 OM2 模型，false 表示不是
 */
ACL_FUNC_VISIBILITY bool AclIsOm2ModelById(uint32_t modelId);

/**
 * @brief 通过文件路径判断是否为 OM2 模型
 * @param modelPath 模型文件路径
 * @return true 表示是 OM2 模型，false 表示不是
 */
ACL_FUNC_VISIBILITY bool AclIsOm2ModelByPath(const char *modelPath);

/**
 * @brief 通过内存数据判断是否为 OM2 模型
 * @param modelData 模型数据指针
 * @param modelSize 模型数据大小
 * @return true 表示是 OM2 模型，false 表示不是
 */
ACL_FUNC_VISIBILITY bool AclIsOm2ModelByData(const void *modelData, size_t modelSize);

/**
 * @brief 通过配置句柄判断是否为 OM2 模型
 * @param handle 模型配置句柄
 * @return true 表示是 OM2 模型，false 表示不是
 */
ACL_FUNC_VISIBILITY bool AclIsOm2ModelByConfig(const aclmdlConfigHandle *handle);

/**
 * @brief 通过 Bundle ID 判断是否为 OM2 Bundle
 * @param bundleId Bundle ID
 * @return true 表示是 OM2 Bundle，false 表示不是
 */
ACL_FUNC_VISIBILITY bool AclIsOm2BundleById(uint32_t bundleId);

#ifdef __cplusplus
}
#endif
namespace acl {

class ACL_FUNC_VISIBILITY AclResourceManagerOm2 {
public:
    ~AclResourceManagerOm2();
    static AclResourceManagerOm2 &GetInstance();

    // OM2 Executor管理
    void AddOm2Executor(uint32_t &modelId,
                        std::unique_ptr<gert::Om2ModelExecutor> &&executor,
                        const std::shared_ptr<gert::RtSession> &rtSession = nullptr);
    uint32_t GenerateModelId();
    void AddOm2ExecutorWithModelId(uint32_t modelId,
                                   std::unique_ptr<gert::Om2ModelExecutor> &&executor,
                                   const std::shared_ptr<gert::RtSession> &rtSession = nullptr);
    std::shared_ptr<gert::Om2ModelExecutor> GetOm2Executor(const uint32_t modelId);
    aclError DeleteOm2Executor(const uint32_t modelId);
    bool IsOm2ModelById(const uint32_t modelId) const;
    bool IsOm2ModelByConfig(const aclmdlConfigHandle *handle) const;
    bool IsOm2ModelByPath(const char *file_path) const;
    bool IsOm2ModelByData(const void *data, size_t size) const;

    // Bundle管理
    void AddBundleSubmodelId(const uint32_t bundleId, uint32_t modelId);
    void DeleteBundleSubmodelId(const uint32_t bundleId, uint32_t modelId);
    aclError SetBundleInfo(const uint32_t bundleId, const BundleModelInfo &bundleInfos);
    aclError GetBundleInfo(const uint32_t bundleId, BundleModelInfo &bundleInfos);
    void DeleteBundleInfo(const uint32_t bundleId);
    bool IsBundleInnerId(const uint32_t modelId) const;
    bool IsOm2BundleById(const uint32_t bundleId) const;

    // RtSession管理
    std::shared_ptr<gert::RtSession> CreateRtSession();
    std::shared_ptr<gert::RtSession> GetRtSession(const uint64_t rtSessionId);

private:
    AclResourceManagerOm2();

private:
    std::unordered_map<uint32_t, std::shared_ptr<gert::Om2ModelExecutor>> om2ExecutorMap_{{0U, nullptr}};
    std::atomic_uint32_t modelIdGenerator_{std::numeric_limits<uint32_t>::max() / 2U};
    std::unordered_map<uint32_t, BundleModelInfo> bundleInfos_;
    std::set<uint32_t> bundleInnerIds_;
    std::atomic_uint64_t sessionIdGenerator_{std::numeric_limits<uint64_t>::max() / 2U};
    std::map<uint64_t, std::shared_ptr<gert::RtSession>> rtSessionMap_;
    mutable std::mutex mutex_;
};

} // namespace acl

#endif // ASCEND_ACL_RESOURCE_MANAGER_OM2_H
