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
#include "runtime/rt_session.h"
#include "framework/runtime/om2_model_executor.h"
#include "model_desc_internal.h"

namespace acl {

AclResourceManagerOm2::AclResourceManagerOm2() {}

AclResourceManagerOm2::~AclResourceManagerOm2() {}

AclResourceManagerOm2 &AclResourceManagerOm2::GetInstance()
{
    static AclResourceManagerOm2 instance;
    return instance;
}

void AclResourceManagerOm2::AddOm2Executor(uint32_t &modelId,
                                           std::unique_ptr<gert::Om2ModelExecutor> &&executor,
                                           const std::shared_ptr<gert::RtSession> &rtSession)
{
    modelId = GenerateModelId();
    AddOm2ExecutorWithModelId(modelId, std::move(executor), rtSession);
}

uint32_t AclResourceManagerOm2::GenerateModelId()
{
    const std::lock_guard<std::mutex> locker(mutex_);
    ++modelIdGenerator_;
    return modelIdGenerator_.load();
}

void AclResourceManagerOm2::AddOm2ExecutorWithModelId(uint32_t modelId,
                                                      std::unique_ptr<gert::Om2ModelExecutor> &&executor,
                                                      const std::shared_ptr<gert::RtSession> &rtSession)
{
    const std::lock_guard<std::mutex> locker(mutex_);
    om2ExecutorMap_[modelId] = std::move(executor);
    if (rtSession != nullptr) {
        rtSessionMap_[modelId] = rtSession;
    }
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

aclError AclResourceManagerOm2::DeleteOm2Executor(const uint32_t modelId)
{
    const std::lock_guard<std::mutex> locker(mutex_);
    const auto iter = om2ExecutorMap_.find(modelId);
    if (iter == om2ExecutorMap_.end()) {
        ACL_LOG_ERROR("model is not loaded, modelId is %u", modelId);
        return ACL_ERROR_GE_EXEC_MODEL_ID_INVALID;
    }
    (void)om2ExecutorMap_.erase(iter);

    const auto it = rtSessionMap_.find(modelId);
    if (it != rtSessionMap_.end()) {
        (void)rtSessionMap_.erase(it);
    }
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

std::shared_ptr<gert::RtSession> AclResourceManagerOm2::CreateRtSession()
{
    const std::lock_guard<std::mutex> locker(mutex_);
    ++sessionIdGenerator_;
    auto sessionId = sessionIdGenerator_.load();
    return std::make_shared<gert::RtSession>(sessionId);
}

std::shared_ptr<gert::RtSession> AclResourceManagerOm2::GetRtSession(const uint64_t rtSessionId)
{
    const std::lock_guard<std::mutex> locker(mutex_);
    const auto it = rtSessionMap_.find(rtSessionId);
    if (it == rtSessionMap_.end()) {
        return nullptr;
    }
    return it->second;
}

bool AclResourceManagerOm2::IsOm2ModelByConfig(const aclmdlConfigHandle *handle) const
{
    if (handle == nullptr) {
        ACL_LOG_WARN("config handle is null, return false");
        return false;
    }

    bool isOm2Model = false;
    ge::Status ret;

    // Try to detect model type from file path first
    if (!handle->loadPath.empty()) {
        ACL_LOG_DEBUG("detecting model type from file path: %s", handle->loadPath.c_str());
        ret = gert::IsOm2Model(handle->loadPath.c_str(), isOm2Model);
        if (ret != ge::SUCCESS) {
            ACL_LOG_WARN("failed to detect model type from file path, result = %u", ret);
            return false;
        }
        ACL_LOG_DEBUG("model type detected from file path: isOm2Model = %d", isOm2Model);
        return isOm2Model;
    }

    // Try to detect model type from memory data
    if (handle->mdlAddr != nullptr && handle->mdlSize > 0) {
        ACL_LOG_DEBUG("detecting model type from memory data, size = %zu", handle->mdlSize);
        ret = gert::IsOm2Model(handle->mdlAddr, handle->mdlSize, isOm2Model);
        if (ret != ge::SUCCESS) {
            ACL_LOG_WARN("failed to detect model type from memory data, result = %u", ret);
            return false;
        }
        ACL_LOG_DEBUG("model type detected from memory data: isOm2Model = %d", isOm2Model);
        return isOm2Model;
    }

    // No valid model path or data
    ACL_LOG_WARN("config handle has no valid model path or data");
    return false;
}

bool AclResourceManagerOm2::IsOm2ModelByPath(const char *file_path) const
{
    if (file_path == nullptr) {
        ACL_LOG_WARN("file_path is nullptr, return false");
        return false;
    }

    bool isOm2Model = false;
    ge::Status ret = gert::IsOm2Model(file_path, isOm2Model);
    if (ret != ge::SUCCESS) {
        ACL_LOG_WARN("failed to check if model is OM2 by path, path=%s, result=%u", file_path, ret);
        return false;
    }

    ACL_LOG_DEBUG("model type detected from file path: isOm2Model = %d", isOm2Model);
    return isOm2Model;
}

bool AclResourceManagerOm2::IsOm2ModelByData(const void *data, size_t size) const
{
    if (data == nullptr || size == 0) {
        ACL_LOG_WARN("data is nullptr or size is 0, return false");
        return false;
    }

    bool isOm2Model = false;
    ge::Status ret = gert::IsOm2Model(data, size, isOm2Model);
    if (ret != ge::SUCCESS) {
        ACL_LOG_WARN("failed to check if model is OM2 by data, size=%zu, result=%u", size, ret);
        return false;
    }

    ACL_LOG_DEBUG("model type detected from memory data: isOm2Model = %d", isOm2Model);
    return isOm2Model;
}

} // namespace acl

// C 接口实现
#ifdef __cplusplus
extern "C" {
#endif

bool AclIsOm2ModelById(uint32_t modelId)
{
    return acl::AclResourceManagerOm2::GetInstance().IsOm2ModelById(modelId);
}

bool AclIsOm2ModelByPath(const char *modelPath)
{
    return acl::AclResourceManagerOm2::GetInstance().IsOm2ModelByPath(modelPath);
}

bool AclIsOm2ModelByData(const void *modelData, size_t modelSize)
{
    return acl::AclResourceManagerOm2::GetInstance().IsOm2ModelByData(modelData, modelSize);
}

bool AclIsOm2ModelByConfig(const aclmdlConfigHandle *handle)
{
    return acl::AclResourceManagerOm2::GetInstance().IsOm2ModelByConfig(handle);
}

bool AclIsOm2BundleById(uint32_t bundleId)
{
    return acl::AclResourceManagerOm2::GetInstance().IsOm2BundleById(bundleId);
}

#ifdef __cplusplus
}
#endif
