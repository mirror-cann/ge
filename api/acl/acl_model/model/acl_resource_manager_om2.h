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

namespace acl {

class ACL_FUNC_VISIBILITY AclResourceManagerOm2 {
public:
    ~AclResourceManagerOm2();
    static AclResourceManagerOm2 &GetInstance();

    bool IsRuntimeV2Enable(bool isModel) const
    {
        return isModel ? enableRuntimeV2ForModel_ : enableRuntimeV2ForSingleOp_;
    }

    // OM2 Executor管理
    void AddOm2Executor(uint32_t &modelId,
                        std::unique_ptr<gert::Om2ModelExecutor> &&executor);
    uint32_t GenerateModelId();
    void AddOm2ExecutorWithModelId(uint32_t modelId,
                                   std::unique_ptr<gert::Om2ModelExecutor> &&executor);
    std::shared_ptr<gert::Om2ModelExecutor> GetOm2Executor(const uint32_t modelId);
    aclError GetOpDescInfo(uint32_t deviceId, uint32_t streamId, uint32_t taskId, ge::OpDescInfo &opDescInfo);
    aclError DeleteOm2Executor(const uint32_t modelId);
    bool IsOm2ModelById(const uint32_t modelId) const;

    // Bundle管理
    void AddBundleSubmodelId(const uint32_t bundleId, uint32_t modelId);
    void DeleteBundleSubmodelId(const uint32_t bundleId, uint32_t modelId);
    aclError SetBundleInfo(const uint32_t bundleId, const BundleModelInfo &bundleInfos);
    aclError GetBundleInfo(const uint32_t bundleId, BundleModelInfo &bundleInfos);
    void DeleteBundleInfo(const uint32_t bundleId);
    bool IsBundleInnerId(const uint32_t modelId) const;
    bool IsOm2BundleById(const uint32_t bundleId) const;

private:
    AclResourceManagerOm2();
    void GetRuntimeV2Env();
private:
    bool enableRuntimeV2ForModel_ = true;
    bool enableRuntimeV2ForSingleOp_ = true;
    std::unordered_map<uint32_t, std::shared_ptr<gert::Om2ModelExecutor>> om2ExecutorMap_{{0U, nullptr}};
    std::atomic_uint32_t modelIdGenerator_{std::numeric_limits<uint32_t>::max() / 2U};
    std::unordered_map<uint32_t, BundleModelInfo> bundleInfos_;
    std::set<uint32_t> bundleInnerIds_;
    mutable std::mutex mutex_;
};

} // namespace acl

#endif // ASCEND_ACL_RESOURCE_MANAGER_OM2_H
