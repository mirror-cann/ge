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
#include <sstream>
#include "framework/common/ge_format_util.h"
#include "utils/string_utils.h"
#include "utils/math_utils.h"
#include "model/acl_resource_manager.h"
#include "common/prof_api_reg.h"
#include "common/error_codes_inner.h"
#include "model/acl_model_impl.h"

namespace acl {
    void ConvertSvecToVec(const ge::SmallVector<int64_t, static_cast<size_t>(ge::kDefaultMaxInputNum)> &svec,
        std::vector<int64_t> &vec)
    {
        vec.resize(svec.size());
        for (size_t i = 0U; i < vec.size(); ++i) {
            vec[i] = svec[i];
        }
    }

    void ConvertVecToSvec(const std::vector<int64_t> &vec,
        ge::SmallVector<int64_t, static_cast<size_t>(ge::kDefaultMaxInputNum)> &svec)
    {
        svec.resize(vec.size());
        for (size_t i = 0U; i < vec.size(); ++i) {
            svec[i] = vec[i];
        }
    }
}

aclTensorDesc::aclTensorDesc(const aclDataType aclTensorDataType,
    const std::initializer_list<int64_t> shape, const aclFormat aclTensorFormat) :
    dataType(aclTensorDataType),
    format(aclTensorFormat),
    dims(shape)
{
    if (acl::AclResourceManager::GetInstance().IsRuntimeV2Enable(false)) { // how to deal with it for model?
        this->storageDims = dims;
    }
}

aclTensorDesc::aclTensorDesc(const aclDataType aclTensorDataType,
    const size_t numDims, const int64_t *const aclTensorDims, const aclFormat aclTensorFormat) :
    dataType(aclTensorDataType),
    format(aclTensorFormat)
{
    for (size_t i = 0U; i < numDims; ++i) {
        this->dims.push_back(*(aclTensorDims + i));
    }
    if (acl::AclResourceManager::GetInstance().IsRuntimeV2Enable(false)) { // how to deal with it for model?
        this->storageDims = dims;
    }
}

void aclTensorDesc::Init(const aclTensorDesc &tensorDesc)
{
    this->dataType = tensorDesc.dataType;
    this->storageFormat = tensorDesc.storageFormat;
    this->format = tensorDesc.format;
    this->dims = tensorDesc.dims;
    this->dimsBackup = tensorDesc.dimsBackup;
    this->storageDims = tensorDesc.storageDims;
    this->storageDimsBackup = tensorDesc.storageDimsBackup;
    this->name = tensorDesc.name;
    this->shapeRange = tensorDesc.shapeRange;
    this->shapeRangeBackup = tensorDesc.shapeRangeBackup;
    this->address = tensorDesc.address;
    this->isConst = tensorDesc.isConst;
    this->isConstBackup = tensorDesc.isConstBackup;
    this->constDataLen = tensorDesc.constDataLen;
    this->constDataLenBackup = tensorDesc.constDataLenBackup;
    this->constDataBuf = tensorDesc.constDataBuf;
    this->constDataBufBackup=tensorDesc.constDataBufBackup;
    this->cachedKey = tensorDesc.cachedKey;
    this->cachedShapeKey = tensorDesc.cachedShapeKey;
    this->memtype = tensorDesc.memtype;
    for (const auto &it : tensorDesc.valueRange) {
        this->valueRange[it.first] = it.second.Copy();
    }
}

aclTensorDesc::aclTensorDesc(const aclTensorDesc &tensorDesc)
{
    Init(tensorDesc);
}

aclTensorDesc &aclTensorDesc::operator=(const aclTensorDesc &tensorDesc)
{
    Init(tensorDesc);
    return *this;
}

std::string aclTensorDesc::DebugString() const
{
    std::stringstream ss;
    ss << "[TensorDesc] ";
    ss << "DataType = " << dataType;
    ss << ", Format = " << format;
    ss << ", StorageFormat = " << storageFormat;
    ss << ", Shape = " << acl::StringUtils::VectorToString(dims);
    ss << ", StorageShape = " << acl::StringUtils::VectorToString(storageDims);
    ss << ", shapeRange = " << acl::StringUtils::VectorToString(shapeRange);
    ss << ", memtype = " << memtype;
    ss << ", isConst = " << isConst;
    if (isConst) {
        ss << " , isConst = true, Const Len = " << (constDataLen / sizeof(int32_t)) << " , Const data = ";
        for (size_t i = 0U; i < (constDataLen / sizeof(int32_t)); ++i) {
            ss <<  *(static_cast<const int32_t*>(constDataBuf.get()) + i);
            ss << ",";
        }
    }
    return ss.str();
}

bool aclTensorDesc::IsDynamicTensor() const
{
    for (size_t i = 0U; i < dims.size(); ++i) {
        if ((dims[i] == acl::UNKNOW_DIM) || (dims[i] == acl::UNKNOW_RANK)) {
            return true;
        }
    }
    if (!valueRange.empty()) {
        return true;
    }
    return false;
}

void aclTensorDesc::UpdateTensorShape(const std::vector<int64_t> &shape)
{
    dims.clear();
    for (size_t i = 0U; i < shape.size(); ++i) {
        dims.emplace_back(shape[i]);
    }
}

void aclTensorDesc::UpdateTensorShapeRange(const std::vector<std::pair<int64_t, int64_t>> &ranges)
{
    shapeRange.clear();
    for (size_t i = 0U; i < ranges.size(); ++i) {
        shapeRange.emplace_back(ranges[i]);
    }
}

bool aclTensorDesc::CheckShapeRange() const
{
    if ((dims.size() > 0U) && (dims.at(0U) == acl::UNKNOW_RANK)) {
        return shapeRange.empty();
    }
    bool isUnkownDim = false;
    for (size_t i = 0U; i < dims.size(); ++i) {
        if (dims[i] == acl::UNKNOW_DIM) {
            isUnkownDim = true;
            break;
        }
    }
    if (isUnkownDim) {
        if (dims.size() != shapeRange.size()) {
            return false;
        }
    }
    return true;
}

bool aclTensorDesc::operator==(const aclTensorDesc *const other) const
{
    // when check model matched failed, we should report WARNING log not ERROR
    if (other == nullptr) {
        ACL_LOG_DEBUG("aclTensorDesc must not be null.");
        return false;
    }

    ACL_CHECK_INT32_EQUAL(static_cast<int32_t>(this->dataType), static_cast<int32_t>(other->dataType));

    ACL_CHECK_INT32_EQUAL(static_cast<int32_t>(this->format), static_cast<int32_t>(other->format));

    ACL_CHECK_INT32_EQUAL(static_cast<int32_t>(this->storageFormat), static_cast<int32_t>(other->storageFormat));

    if (this->dims != other->dims) {
        return false;
    }
    if (this->shapeRange != other->shapeRange) {
        return false;
    }

    ACL_CHECK_INT32_EQUAL(static_cast<int32_t>(this->isConst), static_cast<int32_t>(other->isConst));

    ACL_CHECK_INT32_EQUAL(static_cast<int32_t>(this->memtype), static_cast<int32_t>(other->memtype));

    return true;
}

bool aclTensorDesc::IsSameTensor(const aclTensorDesc *const other) const
{
    // when check model matched failed, we should report WARNING log not ERROR
    if (other == nullptr) {
        ACL_LOG_DEBUG("aclTensorDesc must not be null.");
        return false;
    }

    ACL_CHECK_INT32_EQUAL(static_cast<int32_t>(this->dataType), static_cast<int32_t>(other->dataType));

    ACL_CHECK_INT32_EQUAL(static_cast<int32_t>(this->format), static_cast<int32_t>(other->format));

    ACL_CHECK_INT32_EQUAL(static_cast<int32_t>(this->storageFormat), static_cast<int32_t>(other->storageFormat));

    ACL_CHECK_INT32_EQUAL(static_cast<int32_t>(this->isConst), static_cast<int32_t>(other->isConst));

    ACL_CHECK_INT32_EQUAL(static_cast<int32_t>(this->memtype), static_cast<int32_t>(other->memtype));

    return true;
}














aclError aclTransTensorDescFormatImpl(const aclTensorDesc *srcDesc, aclFormat dstFormat, aclTensorDesc **dstDesc)
{
    ACL_PROFILING_REG(acl::AclProfType::AclTransTensorDescFormat);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(srcDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dstDesc);

    std::vector<int64_t> dims;
    acl::ConvertSvecToVec(srcDesc->dims, dims);
    const ge::Shape shape(dims);
    const auto srcFormat = static_cast<ge::Format>(srcDesc->format);
    const auto dataType = static_cast<ge::DataType >(srcDesc->dataType);
    const ge::TensorDesc desc(shape, srcFormat, dataType);

    std::vector<int64_t> dstShape;
    const auto geRet = ge::GeFormatUtil::TransShape(desc, static_cast<ge::Format>(dstFormat), dstShape);
    if (geRet != ge::SUCCESS) {
        ACL_LOG_CALL_ERROR("[Call][TransShape]invoke TransShape failed. ge result = %u",
                           geRet);
        return ACL_GET_ERRCODE_GE(geRet);
    }

    *dstDesc = aclCreateTensorDesc(srcDesc->dataType, static_cast<int32_t>(dstShape.size()),
                                   dstShape.data(), srcDesc->format);
    if (*dstDesc == nullptr) {
        ACL_LOG_INNER_ERROR("[Create][Desc]aclCreateTensorDesc failed.");
        return ACL_ERROR_BAD_ALLOC;
    }

    return ACL_SUCCESS;
}











void aclTensorDesc::BackupDimsAndShapeRanges()
{
    dimsBackup = dims;
    storageDimsBackup = storageDims;
    shapeRangeBackup = shapeRange;
}

void aclTensorDesc::RecoverDimsAndShapeRanges()
{
    dims = dimsBackup;
    storageDims = storageDimsBackup;
    shapeRange = shapeRangeBackup;
}

void aclTensorDesc::BackupConst()
{
    isConstBackup = isConst;
    constDataBufBackup = constDataBuf;
    constDataLenBackup = constDataLen;
}

void aclTensorDesc::RecoverConst()
{
    isConst = isConstBackup;
    constDataBuf = constDataBufBackup;
    constDataLen = constDataLenBackup;
}

