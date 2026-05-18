/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "acl_resource_manager_om2.h"
#include "common/log_inner.h"
#include "mmpa/mmpa_api.h"
#include "runtime/rt_session.h"
#include "framework/runtime/om2_model_executor.h"
#include "model_desc_internal.h"

namespace acl {

AclResourceManagerOm2::AclResourceManagerOm2()
{
    GetRuntimeV2Env();
}

AclResourceManagerOm2::~AclResourceManagerOm2() {}

AclResourceManagerOm2 &AclResourceManagerOm2::GetInstance()
{
    static AclResourceManagerOm2 instance;
    return instance;
}

void AclResourceManagerOm2::GetRuntimeV2Env()
{
    const char_t *enableRuntimeV2Flag = nullptr;
    MM_SYS_GET_ENV(MM_ENV_ENABLE_RUNTIME_V2, enableRuntimeV2Flag);
    if (enableRuntimeV2Flag != nullptr) {
        if (enableRuntimeV2Flag[0] == '0') { // 0 both model and singleOp disable
            enableRuntimeV2ForModel_ = false;
            enableRuntimeV2ForSingleOp_ = false;
        } else if (enableRuntimeV2Flag[0] == '2') { // 2: model enable, singleOp disable
            enableRuntimeV2ForModel_ = true;
            enableRuntimeV2ForSingleOp_ = false;
        } else {
            enableRuntimeV2ForModel_ = true;
            enableRuntimeV2ForSingleOp_ = true;
        }
    }
    ACL_LOG_EVENT("runtime v2 flag : model flag = %d, singleOp flag = %d",
                  static_cast<int32_t>(enableRuntimeV2ForModel_),
                  static_cast<int32_t>(enableRuntimeV2ForSingleOp_));
}

void AclResourceManagerOm2::AddOm2Executor(uint32_t &modelId,
                                           std::unique_ptr<gert::Om2ModelExecutor> &&executor)
{
    modelId = GenerateModelId();
    AddOm2ExecutorWithModelId(modelId, std::move(executor));
}

uint32_t AclResourceManagerOm2::GenerateModelId()
{
    const std::lock_guard<std::mutex> locker(mutex_);
    ++modelIdGenerator_;
    return modelIdGenerator_.load();
}

void AclResourceManagerOm2::AddOm2ExecutorWithModelId(uint32_t modelId,
                                                      std::unique_ptr<gert::Om2ModelExecutor> &&executor)
{
    const std::lock_guard<std::mutex> locker(mutex_);
    om2ExecutorMap_[modelId] = std::move(executor);
}

std::shared_ptr<gert::Om2ModelExecutor> AclResourceManagerOm2::GetOm2Executor(const uint32_t modelId)
{
    const std::lock_guard<std::mutex> locker(mutex_);
    const auto iter = om2ExecutorMap_.find(modelId);
    if (iter == om2ExecutorMap_.end()) {
        return nullptr;
    }
    return iter->second;
}

aclError AclResourceManagerOm2::GetOpDescInfo(const uint32_t deviceId, const uint32_t streamId,
                                              const uint32_t taskId, ge::OpDescInfo &opDescInfo)
{
    std::vector<std::shared_ptr<gert::Om2ModelExecutor>> executors;
    {
        const std::lock_guard<std::mutex> locker(mutex_);
        executors.reserve(om2ExecutorMap_.size());
        for (const auto &iter : om2ExecutorMap_) {
            if (iter.second != nullptr) {
                executors.emplace_back(iter.second);
            }
        }
    }

    for (const auto &executor : executors) {
        const ge::Status ret = executor->GetOpDescInfo(deviceId, streamId, taskId, opDescInfo);
        if (ret == ge::SUCCESS) {
            return ACL_SUCCESS;
        }
    }
    ACL_LOG_INFO("[OM2][Get][OpDescInfo] cannot find op desc info, deviceId[%u], streamId[%u], taskId[%u]",
                 deviceId, streamId, taskId);
    return ACL_ERROR_GE_FAILURE;
}

aclError AclResourceManagerOm2::DeleteOm2Executor(const uint32_t modelId)
{
    const std::lock_guard<std::mutex> locker(mutex_);
    const auto iter = om2ExecutorMap_.find(modelId);
    if (iter == om2ExecutorMap_.end()) {
        ACL_LOG_ERROR("model is not loaded, modelId is %u", modelId);
        return ACL_ERROR_GE_EXEC_MODEL_ID_INVALID;
    }
    (void)om2ExecutorMap_.erase(iter);
    return ACL_SUCCESS;
}

bool AclResourceManagerOm2::IsOm2ModelById(const uint32_t modelId) const
{
    const std::lock_guard<std::mutex> locker(mutex_);
    return (modelId != 0U) && (om2ExecutorMap_.count(modelId) > 0U);
}

void AclResourceManagerOm2::AddBundleSubmodelId(const uint32_t bundleId, uint32_t modelId)
{
    const std::lock_guard<std::mutex> locker(mutex_);
    bundleInfos_[bundleId].loadedSubModelIdSet.insert(modelId);
    (void)bundleInnerIds_.insert(modelId);
}

void AclResourceManagerOm2::DeleteBundleSubmodelId(const uint32_t bundleId, uint32_t modelId)
{
    const std::lock_guard<std::mutex> locker(mutex_);
    bundleInfos_[bundleId].loadedSubModelIdSet.erase(modelId);
    (void)bundleInnerIds_.erase(modelId);
}

aclError AclResourceManagerOm2::SetBundleInfo(const uint32_t bundleId, const BundleModelInfo &bundleInfos)
{
    const std::lock_guard<std::mutex> locker(mutex_);
    bundleInfos_[bundleId] = bundleInfos;
    for (const auto &modelId : bundleInfos.loadedSubModelIdSet) {
        (void)bundleInnerIds_.insert(modelId);
    }
    return ACL_SUCCESS;
}

aclError AclResourceManagerOm2::GetBundleInfo(const uint32_t bundleId, BundleModelInfo &bundleInfos)
{
    const std::lock_guard<std::mutex> locker(mutex_);
    const auto it = bundleInfos_.find(bundleId);
    if (it == bundleInfos_.end()) {
        ACL_LOG_ERROR("This model %u is not bundle model, can not get bundle info.", bundleId);
        return ACL_ERROR_INVALID_BUNDLE_MODEL_ID;
    }
    bundleInfos = it->second;
    return ACL_SUCCESS;
}

void AclResourceManagerOm2::DeleteBundleInfo(const uint32_t bundleId)
{
    const std::lock_guard<std::mutex> locker(mutex_);
    const auto it = bundleInfos_.find(bundleId);
    if (it != bundleInfos_.end()) {
        for (const auto &id : it->second.loadedSubModelIdSet) {
            (void)bundleInnerIds_.erase(id);
        }
        (void)bundleInfos_.erase(it);
    }
}

bool AclResourceManagerOm2::IsBundleInnerId(const uint32_t modelId) const
{
    const std::lock_guard<std::mutex> locker(mutex_);
    return (bundleInnerIds_.count(modelId) > 0U);
}

bool AclResourceManagerOm2::IsOm2BundleById(const uint32_t bundleId) const
{
    const std::lock_guard<std::mutex> locker(mutex_);
    return bundleInfos_.find(bundleId) != bundleInfos_.end();
}

} // namespace acl
