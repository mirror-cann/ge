/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "model_common.h"

#include <cstdio>
#include <cstring>
#include <unordered_set>
#include <vector>
#include "common/log_inner.h"
#include "model_desc_internal.h"
#include "framework/common/types.h"
#include "securec.h"

namespace {
constexpr size_t DYNAMIC_BATCH_SIZE = 1U;
constexpr size_t DYNAMIC_HW_SIZE = 2U;

// OM2 supported load config options
const std::unordered_set<aclmdlConfigAttr> kOm2SupportedLoadConfigOpts = {
    ACL_MDL_LOAD_TYPE_SIZET,
    ACL_MDL_PATH_PTR,
    ACL_MDL_MEM_ADDR_PTR,
    ACL_MDL_MEM_SIZET,
    ACL_MDL_WEIGHT_ADDR_PTR,
    ACL_MDL_WEIGHT_SIZET,
    ACL_MDL_WORKSPACE_ADDR_PTR,
    ACL_MDL_WORKSPACE_SIZET,
    ACL_MDL_WEIGHT_PATH_PTR
};
} // namespace

aclError CheckOm2UserLoadConfigOptValid(const aclmdlConfigHandle* handle) {
    ACL_REQUIRES_NOT_NULL(handle);
    for (const aclmdlConfigAttr& attr : handle->attrState) {
        if (kOm2SupportedLoadConfigOpts.find(attr) == kOm2SupportedLoadConfigOpts.end()) {
            ACL_LOG_ERROR("[Check][ConfigOpt]config opt [%d] is not supported in om2",  static_cast<int>(attr));
            return ACL_ERROR_INVALID_PARAM;
        }
    }
    return ACL_SUCCESS;
}

namespace acl {
// Parse batch info helper (inline implementation)
static aclError ParseBatchInfoInline(aclmdlDesc * const modelDesc, const int32_t dynamicType,
                                     const std::vector<std::vector<int64_t>> &batchInfo)
{
    const uint32_t modelId = modelDesc->modelId;
    if (dynamicType == static_cast<int32_t>(ge::DYNAMIC_DIMS)) {
        // dynamic dims, size can be [1, 4]
        const size_t dimCount = batchInfo[0U].size();
        for (size_t i = 0U; i < batchInfo.size(); ++i) {
            if (batchInfo[i].size() != dimCount) {
                ACL_LOG_INNER_ERROR("[Check][Size]Get dynamic model info invalid, model id[%u], one dim count is %zu "
                    "while another is %zu", modelId, dimCount, batchInfo[i].size());
                modelDesc->dynamicDims.clear();
                return ACL_ERROR_GE_FAILURE;
            }
            std::vector<uint64_t> oneDims;
            for (size_t j = 0U; j < dimCount; ++j) {
                oneDims.push_back(static_cast<uint64_t>(batchInfo[i][j]));
            }
            modelDesc->dynamicDims.push_back(oneDims);
        }
    } else if (batchInfo[0U].size() == DYNAMIC_BATCH_SIZE) {
        // dynamic batch, size is 1
        for (size_t i = 0U; i < batchInfo.size(); ++i) {
            if (batchInfo[i].size() != DYNAMIC_BATCH_SIZE) {
                ACL_LOG_INNER_ERROR("[Check][Size]get dynamic model info invalid, model id[%u]", modelId);
                modelDesc->dynamicBatch.clear();
                return ACL_ERROR_GE_FAILURE;
            }
            modelDesc->dynamicBatch.push_back(static_cast<uint64_t>(batchInfo[i][0U]));
        }
    } else if (batchInfo[0U].size() == DYNAMIC_HW_SIZE) {
        // dynamic hw, size is 2
        for (size_t i = 0U; i < batchInfo.size(); ++i) {
            if (batchInfo[i].size() != DYNAMIC_HW_SIZE) {
                ACL_LOG_INNER_ERROR("[Check][Size]get dynamic model info invalid, model id[%u]", modelId);
                modelDesc->dynamicHW.clear();
                return ACL_ERROR_GE_FAILURE;
            }
            modelDesc->dynamicHW.push_back({static_cast<uint64_t>(batchInfo[i][0U]),
                                            static_cast<uint64_t>(batchInfo[i][1U])});
        }
    } else {
        ACL_LOG_INNER_ERROR("[Get][DynamicModel]get dynamic model info invalid, model id[%u]", modelId);
        return ACL_ERROR_GE_FAILURE;
    }
    return ACL_SUCCESS;
}


static bool CheckMdlLoadConfigFromFile(const aclmdlConfigHandle *const handle)
{
    if (handle->attrState.find(ACL_MDL_PATH_PTR) == handle->attrState.end()) {
        ACL_LOG_ERROR("[Check][Type]model load type[%zu]: model path is not set in aclmdlConfigHandle "
                      "when load type is from file", handle->mdlLoadType);
        const std::string errMsg = "model path is not set in aclmdlConfigHandle when load type is from file";
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
            std::vector<const char *>({"param", "value", "reason"}),
            std::vector<const char *>({"handle", "inner model path", errMsg.c_str()}));
        return false;
    }
    if (handle->attrState.find(ACL_MDL_WEIGHT_PATH_PTR) != handle->attrState.end()) {
        ACL_LOG_ERROR("[Check][Type]model load type[%zu]: should not set ACL_MDL_WEIGHT_PATH_PTR",
            handle->mdlLoadType);
        const std::string errMsg = "should not set ACL_MDL_WEIGHT_PATH_PTR";
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
            std::vector<const char *>({"param", "value", "reason"}),
            std::vector<const char *>({"handle", "weight path", errMsg.c_str()}));
        return false;
    }
    return true;
}

static bool CheckMdlLoadConfigFromMem(const aclmdlConfigHandle *const handle)
{
    if (handle->attrState.find(ACL_MDL_MEM_ADDR_PTR) == handle->attrState.end()) {
        ACL_LOG_ERROR("[Check][Type]model load type[%zu]: model memory ptr is not set in aclmdlConfigHandle",
            handle->mdlLoadType);
        const std::string errMsg = "model memory ptr is not set in aclmdlConfigHandle when load type is from mem";
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
            std::vector<const char *>({"param", "value", "reason"}),
            std::vector<const char *>({"handle", "inner model memory", errMsg.c_str()}));
        return false;
    }

    if (handle->attrState.find(ACL_MDL_MEM_SIZET) == handle->attrState.end()) {
        ACL_LOG_ERROR("[Check][Type]model load type[%zu]: model memory size is not set in aclmdlConfigHandle",
            handle->mdlLoadType);
        const std::string errMsg = "model memory size is not set in aclmdlConfigHandle when load type is from mem";
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
            std::vector<const char *>({"param", "value", "reason"}),
            std::vector<const char *>({"handle", "inner model memory size", errMsg.c_str()}));
        return false;
    }
    if ((handle->mdlLoadType == static_cast<size_t>(ACL_MDL_LOAD_FROM_MEM_WITH_MEM)) &&
            (handle->attrState.find(ACL_MDL_WEIGHT_PATH_PTR) != handle->attrState.end())) {
        ACL_LOG_ERROR("[Check][Type]model load type[%zu]: should not set ACL_MDL_WEIGHT_PATH_PTR",
            handle->mdlLoadType);
        const std::string errMsg = "should not set ACL_MDL_WEIGHT_PATH_PTR in aclmdlConfigHandle "
                                   "when load type is ACL_MDL_LOAD_FROM_MEM_WITH_MEM";
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
            std::vector<const char *>({"param", "value", "reason"}),
            std::vector<const char *>({"handle", "inner weight path", errMsg.c_str()}));
        return false;
    }
    return true;
}

static bool CheckMdlLoadConfigWithQ(const aclmdlConfigHandle *const handle)
{
        if (handle->attrState.find(ACL_MDL_INPUTQ_ADDR_PTR) == handle->attrState.end()) {
            ACL_LOG_ERROR("[Check][Type]model load type[%zu]: inputQ ptr is not set in aclmdlConfigHandle",
                handle->mdlLoadType);
            const std::string errMsg = "ACL_MDL_INPUTQ_ADDR_PTR is not set in aclmdlConfigHandle "
                                       "when load type is with queue";
            acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
                std::vector<const char *>({"param", "value", "reason"}),
                std::vector<const char *>({"handle", "inner inputq addr", errMsg.c_str()}));
            return false;
        }

        if (handle->attrState.find(ACL_MDL_INPUTQ_NUM_SIZET) == handle->attrState.end()) {
            ACL_LOG_ERROR("[Check][Type]model load type[%zu]: inputQ num is not set in aclmdlConfigHandle",
                handle->mdlLoadType);
            const std::string errMsg = "ACL_MDL_INPUTQ_NUM_SIZET is not set in aclmdlConfigHandle "
                                       "when load type is with queue";
            acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
                std::vector<const char *>({"param", "value", "reason"}),
                std::vector<const char *>({"handle", "inner inputq num", errMsg.c_str()}));
            return false;
        }

        if (handle->attrState.find(ACL_MDL_OUTPUTQ_ADDR_PTR) == handle->attrState.end()) {
            ACL_LOG_ERROR("[Check][Type]model load type[%zu]: outputQ ptr is not set in aclmdlConfigHandle",
                handle->mdlLoadType);
            const std::string errMsg = "ACL_MDL_OUTPUTQ_ADDR_PTR is not set in aclmdlConfigHandle "
                                       "when load type is with queue";
            acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
                std::vector<const char *>({"param", "value", "reason"}),
                std::vector<const char *>({"handle", "inner outputq addr", errMsg.c_str()}));
            return false;
        }

        if (handle->attrState.find(ACL_MDL_OUTPUTQ_NUM_SIZET) == handle->attrState.end()) {
            ACL_LOG_ERROR("[Check][Type]model load type[%zu]: outputQ num is not set in aclmdlConfigHandle",
                handle->mdlLoadType);
            const std::string errMsg = "ACL_MDL_OUTPUTQ_NUM_SIZET is not set in aclmdlConfigHandle "
                                       "when load type is with queue";
            acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
                std::vector<const char *>({"param", "value", "reason"}),
                std::vector<const char *>({"handle", "inner outputq num", errMsg.c_str()}));
            return false;
        }

        return true;
}

bool CheckMdlConfigHandle(const aclmdlConfigHandle *const handle)
{
    if (handle->attrState.find(ACL_MDL_LOAD_TYPE_SIZET) == handle->attrState.end()) {
        ACL_LOG_ERROR("[Find][Type]model load type is not set in aclmdlConfigHandle");
        const std::string errMsg = "ACL_MDL_LOAD_TYPE_SIZET is not set in aclmdlConfigHandle";
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
            std::vector<const char *>({"param", "value", "reason"}),
            std::vector<const char *>({"handle", "inner load type", errMsg.c_str()}));
        return false;
    }

    if ((handle->mdlLoadType == static_cast<size_t>(ACL_MDL_LOAD_FROM_FILE)) ||
        (handle->mdlLoadType == static_cast<size_t>(ACL_MDL_LOAD_FROM_FILE_WITH_MEM))) {
        if (!CheckMdlLoadConfigFromFile(handle)) {
            return false;
        }
        }

    if ((handle->mdlLoadType == static_cast<size_t>(ACL_MDL_LOAD_FROM_MEM)) ||
        (handle->mdlLoadType == static_cast<size_t>(ACL_MDL_LOAD_FROM_MEM_WITH_MEM))) {
        if ((!CheckMdlLoadConfigFromMem(handle))) {
            return false;
        }
        }

    if (handle->mdlLoadType == static_cast<size_t>(ACL_MDL_LOAD_FROM_FILE_WITH_Q)) {
        if ((!CheckMdlLoadConfigFromFile(handle)) || (!CheckMdlLoadConfigWithQ(handle))) {
            return false;
        }
    }

    if (handle->mdlLoadType == static_cast<size_t>(ACL_MDL_LOAD_FROM_MEM_WITH_Q)) {
        if ((!CheckMdlLoadConfigFromMem(handle)) || (!CheckMdlLoadConfigWithQ(handle))) {
            return false;
        }
    }
    return true;
}
} // namespace acl

aclError GetDynamicTensorInfoHelp(aclmdlDesc *const modelDesc, const int32_t dynamicType,
                                  const std::vector<std::vector<int64_t>> &batchInfo)
{
    if (batchInfo.empty()) {
        ACL_LOG_INFO("model is not dynamic, batchInfo is empty, modelId[%u]", modelDesc->modelId);
        return ACL_SUCCESS;
    }

    ACL_LOG_INFO("model is dynamic, modelId[%u]", modelDesc->modelId);
    const aclError retVal = acl::ParseBatchInfoInline(modelDesc, dynamicType, batchInfo);
    if (retVal != ACL_SUCCESS) {
        ACL_LOG_INNER_ERROR("[Parse][BatchInfo]get model dynamic info failed, result[%d], model id[%u]",
            retVal, modelDesc->modelId);
        return retVal;
    }

    return ACL_SUCCESS;
}

aclError GetDimsFromModelDesc(const aclmdlDesc *const modelDesc, const TensorType tensorType,
                              const DimsType dimsType, const size_t idx, aclmdlIODims *const dims)
{
    ACL_REQUIRES_NOT_NULL(dims);
    std::vector<aclmdlTensorDesc> desc;
    if (tensorType == TensorType::INPUT_TENSOR_TYPE) {
        desc = modelDesc->inputDesc;
    } else {
        desc = modelDesc->outputDesc;
    }

    const size_t descSize = desc.size();
    if (idx >= descSize) {
        ACL_LOG_INNER_ERROR("[Check][Params]GetDims failed, index[%zu] can not greater than or equal to tensor "
            "size[%zu]", idx, descSize);
        return ACL_ERROR_INVALID_PARAM;
    }

    const aclmdlTensorDesc &tensorDesc = desc[idx];
    std::vector<int64_t> tensorDims;
    if (dimsType == DimsType::DIMS_TYPE_V1) {
        tensorDims = tensorDesc.dims;
    } else if (dimsType == DimsType::DIMS_TYPE_V2) {
        tensorDims = tensorDesc.dimsV2;
    } else {
        ACL_LOG_INNER_ERROR("[Check][dimsType]dims type[%d] is invalid", static_cast<int32_t>(dimsType));
        return ACL_ERROR_FAILURE;
    }

    const size_t dimSize = tensorDims.size();
    if (dimSize > static_cast<size_t>(ACL_MAX_DIM_CNT)) {
        ACL_LOG_INNER_ERROR("[Check][dimSize]get dims failed, dims count[%zu] can not larger than max[%d]",
            dims->dimCount, ACL_MAX_DIM_CNT);
        return ACL_ERROR_STORAGE_OVER_LIMIT;
    }
    dims->dimCount = dimSize;

    for (size_t i = 0U; i < dimSize; ++i) {
        dims->dims[i] = tensorDims[i];
    }

    // Copy tensor name
    const auto ret = strncpy_s(dims->name, ACL_MAX_TENSOR_NAME_LEN, tensorDesc.name.c_str(), tensorDesc.name.size());
    if (ret != EOK) {
        ACL_LOG_INNER_ERROR("[Copy][Str]call strncpy_s failed, result = %d", ret);
        return ACL_ERROR_FAILURE;
    }

    return ACL_SUCCESS;
}

