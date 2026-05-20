/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "tensor_desc_internal.h"
#include "model/acl_model_impl_om2.h"
#include "utils/math_utils.h"
#include "common/prof_api_reg.h"

aclTensorDesc *aclCreateTensorDescImplOm2(aclDataType dataType,
                                          int numDims,
                                          const int64_t *dims,
                                          aclFormat format)
{
    ACL_PROFILING_REG(acl::AclProfType::AclCreateTensorDesc);
    if (numDims < 0) {
        ACL_LOG_ERROR("[Check][NumDims]numDims[%d] is smaller than 0", numDims);
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
            std::vector<const char *>({"param", "value", "reason"}),
            std::vector<const char *>({"numDims", std::to_string(numDims).c_str(),
            "can't smaller than 0"}));
        return nullptr;
    }
    if ((numDims > 0) && (dims == nullptr)) {
        ACL_LOG_ERROR("[Check][Dims]dims is null while numDims[%d] > 0", numDims);
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
            std::vector<const char *>({"param", "value", "reason"}),
            std::vector<const char *>({"numDims", std::to_string(numDims).c_str(),
            "dims is null while numDims > 0"}));
        return nullptr;
    }

    // 1 respresents that creates one tensor
    return new(std::nothrow) aclTensorDesc[1]{{dataType, static_cast<size_t>(numDims), dims, format}};
}

void aclDestroyTensorDescImplOm2(const aclTensorDesc *desc)
{
    ACL_PROFILING_REG(acl::AclProfType::AclDestroyTensorDesc);
    ACL_DELETE_ARRAY_AND_SET_NULL(desc);
}

aclError aclSetTensorShapeRangeImplOm2(aclTensorDesc* desc, size_t dimsCount, int64_t dimsRange[][ACL_TENSOR_SHAPE_RANGE_NUM])
{
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(desc);
    // dimsCount should be equal to length of array dimsRange
    desc->shapeRange.clear();
    for (size_t i = 0U; i < dimsCount; ++i) {
        desc->shapeRange.emplace_back(std::make_pair(dimsRange[i][0], dimsRange[i][1]));
    }
    return ACL_SUCCESS;
}

aclError aclSetTensorValueRangeImplOm2(aclTensorDesc *desc, size_t valueCount,
    int64_t valueRange[][ACL_TENSOR_VALUE_RANGE_NUM])
{
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(desc);
    desc->valRange.clear();
    for (size_t i = 0U; i < valueCount; ++i) {
        desc->valRange.emplace_back(std::make_pair(valueRange[i][0], valueRange[i][1]));
    }
    return ACL_SUCCESS;
}

aclDataType aclGetTensorDescTypeImplOm2(const aclTensorDesc *desc)
{
    if (desc == nullptr) {
        ACL_LOG_ERROR("[Check][Desc]desc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
            std::vector<const char *>({"param"}), std::vector<const char *>({"desc"}));
        return ACL_DT_UNDEFINED;
    }

    return desc->dataType;
}

aclFormat aclGetTensorDescFormatImplOm2(const aclTensorDesc *desc)
{
    if (desc == nullptr) {
        ACL_LOG_ERROR("[Check][Desc]desc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
            std::vector<const char *>({"param"}), std::vector<const char *>({"desc"}));
        return ACL_FORMAT_UNDEFINED;
    }

    return desc->format;
}

size_t aclGetTensorDescElementCountImplOm2(const aclTensorDesc *desc)
{
    if (desc == nullptr) {
        ACL_LOG_ERROR("[Check][Desc]desc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
            std::vector<const char *>({"param"}), std::vector<const char *>({"desc"}));
        return 0U;
    }

    if (desc->dims.empty()) {
        return static_cast<size_t>(1);
    }

    size_t elementCount = 1U;
    for (const int64_t dim : desc->dims) {
        if (dim < 0) { // dim cannot be less than 0
            ACL_LOG_INNER_ERROR("[Check][Dim]invalid dim value %ld", dim);
            return 0U;
        }
        const aclError ret = acl::CheckSizeTMultiOverflow(elementCount, static_cast<size_t>(dim), elementCount);
        if (ret != ACL_SUCCESS) {
            return 0U;
        }
    }

    return elementCount;
}

size_t aclGetTensorDescSizeImplOm2(const aclTensorDesc *desc)
{
    if (desc == nullptr) {
        ACL_LOG_ERROR("[Check][Desc]desc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
            std::vector<const char *>({"param"}), std::vector<const char *>({"desc"}));
        return 0U;
    }
    size_t size = 0U;
    const size_t descCount = aclGetTensorDescElementCountImplOm2(desc);
    const size_t typeSize = aclDataTypeSize(desc->dataType);
    (void)acl::CheckSizeTMultiOverflow(descCount, typeSize, size);
    return size;
}

size_t aclGetTensorDescNumDimsImplOm2(const aclTensorDesc *desc)
{
    if (desc == nullptr) {
        ACL_LOG_ERROR("[Check][Desc]desc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
            std::vector<const char *>({"param"}), std::vector<const char *>({"desc"}));
        return 0U;
    }
    if ((desc->dims.size() > 0U) && (desc->dims[0U] == acl::UNKNOW_RANK)) {
        return static_cast<size_t>(ACL_UNKNOWN_RANK);
    }
    return desc->dims.size();
}

aclError aclGetTensorDescDimRangeImplOm2(const aclTensorDesc* desc,
                                         size_t index,
                                         size_t dimRangeNum,
                                         int64_t *dimRange)
{
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(desc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dimRange);
    if (index >= desc->shapeRange.size()) {
        ACL_LOG_ERROR("[Check][Index]index out of range. index = %zu, numDims = %zu",
            index, desc->shapeRange.size());
        const std::string errMsg = acl::AclErrorLogManager::FormatStr("index out of range. numDims = %zu",
            desc->shapeRange.size());
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
            std::vector<const char *>({"param", "value", "reason"}),
            std::vector<const char *>({"index", std::to_string(index).c_str(), errMsg.c_str()}));
        return ACL_ERROR_INVALID_PARAM;
    }
    if (dimRangeNum < static_cast<size_t>(ACL_TENSOR_SHAPE_RANGE_NUM)) {
        ACL_LOG_ERROR("[Check][DimRangeNum]dimRangeNum cannot be less than 2. dimRangeNum = %zu",
            dimRangeNum);
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
            std::vector<const char *>({"param", "value", "reason"}),
            std::vector<const char *>({"dimRangeNum", std::to_string(dimRangeNum).c_str(),
            "cannot be less than 2"}));
        return ACL_ERROR_INVALID_PARAM;
    }
    *dimRange = desc->shapeRange[index].first;
    *(dimRange + 1) = desc->shapeRange[index].second;

    return ACL_SUCCESS;
}

int64_t aclGetTensorDescDimImplOm2(const aclTensorDesc *desc, size_t index)
{
    if (desc == nullptr) {
        ACL_LOG_ERROR("[Check][Desc]desc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
            std::vector<const char *>({"param"}), std::vector<const char *>({"desc"}));
        return -1;
    }

    if (index >= desc->dims.size()) {
        ACL_LOG_ERROR("[Check][Index]index out of range. index = %zu, numDims = %zu",
            index, desc->dims.size());
        const std::string errMsg = acl::AclErrorLogManager::FormatStr("index out of range. "
            "numDims = %zu", desc->dims.size());
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
            std::vector<const char *>({"param", "value", "reason"}),
            std::vector<const char *>({"index", std::to_string(index).c_str(), errMsg.c_str()}));
        return -1;
    }

    return desc->dims[index];
}

aclError aclGetTensorDescDimV2ImplOm2(const aclTensorDesc *desc, size_t index, int64_t *dimSize)
{
    if (desc == nullptr) {
        ACL_LOG_ERROR("[Check][Desc]desc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
            std::vector<const char *>({"param"}),
            std::vector<const char *>({"desc"}));
        return ACL_ERROR_INVALID_PARAM;
    }
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dimSize);
    if (index >= desc->dims.size()) {
        ACL_LOG_ERROR("[Check][Index]index out of range. index = %zu, numDims = %zu",
            index, desc->dims.size());
        const std::string errMsg = acl::AclErrorLogManager::FormatStr("index out of range. numDims = %zu",
            desc->dims.size());
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
            std::vector<const char *>({"param", "value", "reason"}),
            std::vector<const char *>({"index", std::to_string(index).c_str(), errMsg.c_str()}));
        return ACL_ERROR_INVALID_PARAM;
    }
    *dimSize = desc->dims[index];

    return ACL_SUCCESS;
}

void aclSetTensorDescNameImplOm2(aclTensorDesc *desc, const char *name)
{
    if (desc == nullptr) {
        ACL_LOG_ERROR("[Check][Desc]desc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
            std::vector<const char *>({"param"}), std::vector<const char *>({"desc"}));
        return;
    }

    if (name == nullptr) {
        desc->name = "";
        return;
    }

    desc->name = std::string(name);
}

const char *aclGetTensorDescNameImplOm2(aclTensorDesc *desc)
{
    if (desc == nullptr) {
        return "";
    }

    return desc->name.c_str();
}

aclError aclTransTensorDescFormatImplOm2(const aclTensorDesc *srcDesc, aclFormat dstFormat, aclTensorDesc **dstDesc)
{
    (void)srcDesc;
    (void)dstFormat;
    (void)dstDesc;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclSetTensorStorageFormatImplOm2(aclTensorDesc *desc, aclFormat format)
{
    if (desc != nullptr) {
        desc->storageFormat = format;
    }

    return ACL_SUCCESS;
}

aclError aclSetTensorStorageShapeImplOm2(aclTensorDesc *desc, int numDims, const int64_t *dims)
{
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(desc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dims);

    desc->storageDims.clear();
    for (int32_t i = 0; i < static_cast<int32_t>(numDims); ++i) {
        desc->storageDims.push_back(*(dims + i));
    }

    return ACL_SUCCESS;
}

aclError aclSetTensorFormatImplOm2(aclTensorDesc *desc, aclFormat format)
{
    if (desc != nullptr) {
        desc->storageFormat = format;
    }

    return ACL_SUCCESS;
}

aclError aclSetTensorShapeImplOm2(aclTensorDesc *desc, int numDims, const int64_t *dims)
{
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(desc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dims);

    desc->storageDims.clear();
    for (int32_t i = 0; i < static_cast<int32_t>(numDims); ++i) {
        desc->storageDims.push_back(*(dims + i));
    }

    return ACL_SUCCESS;
}

aclError aclSetTensorOriginFormatImplOm2(aclTensorDesc *desc, aclFormat format)
{
    if (desc != nullptr) {
        desc->format = format;
    }

    return ACL_SUCCESS;
}

aclError aclSetTensorOriginShapeImplOm2(aclTensorDesc *desc, int numDims, const int64_t *dims)
{
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(desc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dims);

    desc->dims.clear();
    for (int32_t i = 0; i < numDims; ++i) {
        desc->dims.push_back(*(dims + i));
    }

    return ACL_SUCCESS;
}

aclTensorDesc *aclGetTensorDescByIndexImplOm2(aclTensorDesc *desc, size_t index)
{
    ACL_REQUIRES_NOT_NULL_RET_NULL_INPUT_REPORT(desc);
    return (desc + index);
}

void *aclGetTensorDescAddressImplOm2(const aclTensorDesc *desc)
{
    ACL_REQUIRES_NOT_NULL_RET_NULL_INPUT_REPORT(desc);
    return desc->address;
}

aclError aclSetTensorDynamicInputImplOm2(aclTensorDesc *desc, const char *dynamicInputName)
{
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(desc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dynamicInputName);

    desc->dynamicInputName = std::string(dynamicInputName);
    return ACL_SUCCESS;
}

aclError aclSetTensorConstImplOm2(aclTensorDesc *desc, void *dataBuffer, size_t length)
{
    ACL_LOG_INFO("start to execute aclSetTensorConst");
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(desc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dataBuffer);
    if (length == 0U) {
        ACL_LOG_ERROR("[Check][Length]The length of const dataBuffer is invalid. size = %zu", length);
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
            std::vector<const char *>({"param"}),
            std::vector<const char *>({"desc"}));
        return ACL_ERROR_INVALID_PARAM;
    }
    desc->isConst = true;

    auto *constData = new (std::nothrow) char[length];
    ACL_REQUIRES_NOT_NULL(constData);
    if (memcpy_s(constData, length, dataBuffer, length) != EOK) {
        ACL_LOG_INNER_ERROR("[Copy][Data]Copy const data failed. size = %zu", length);
        ACL_DELETE_ARRAY_AND_SET_NULL(constData);
        return ACL_ERROR_FAILURE;
    }

    desc->constDataBuf.reset(constData, [](const char_t* const ptr)
        { delete[]ptr; ACL_LOG_DEBUG("delete const data in aclSetTensorConst"); });
    desc->constDataLen = length;
    return ACL_SUCCESS;
}

aclError aclSetTensorPlaceMentImplOm2(aclTensorDesc *desc, aclMemType memType)
{
    ACL_LOG_INFO("start to execute aclSetTensorPlaceMent, memType is %d", static_cast<int32_t>(memType));
    ACL_REQUIRES_NOT_NULL(desc);
    desc->memtype = memType;
    return ACL_SUCCESS;
}
