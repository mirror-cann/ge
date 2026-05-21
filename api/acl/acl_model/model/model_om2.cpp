/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <base.h>
#include <queue>

#include "model/acl_model_impl_om2.h"
#include "model_desc_internal.h"
#include "common/prof_api_reg.h"
#include "common/log_inner.h"
#include "common/error_codes_inner.h"
#include "acl_resource_manager_om2.h"
#include "framework/runtime/om2_context.h"
#include "framework/runtime/om2_model_executor.h"
#include "types/tensor_desc_internal.h"
#include "types/data_buffer_internal.h"
#include "acl/acl_rt.h"
#include "model_common.h"
#include "model_config.h"
#include "securec.h"
#include "utils/math_utils.h"
#include "utils/string_utils.h"

namespace {
constexpr int32_t DEFAULT_SYNC_TIMEOUT = -1;

// Helper function to set device ID in OM2 model load argument
aclError SetOm2ModelLoadArgDevice(gert::Om2ModelLoadArg& loadArgs) {
    int32_t deviceId = -1;
    const auto ret = aclrtGetDevice(&deviceId);
    if ((ret != ACL_SUCCESS) || (deviceId < 0)) {
        ACL_LOG_ERROR("[OM2][GetDevice] aclrtGetDevice failed, ret[%u], deviceId[%d]", ret, deviceId);
        return ret == ACL_SUCCESS ? ACL_ERROR_INVALID_PARAM : ret;
    }
    loadArgs.device_id = deviceId;
    return ACL_SUCCESS;
}

aclError CreateAclTensorDescFromOpDescInfo(const ge::OpDescInfo& opDescInfo, aclTensorDesc** inputDesc,
                                           size_t* numInputs, aclTensorDesc** outputDesc, size_t* numOutputs) {
    const size_t inputNum = opDescInfo.input_format.size();
    const size_t outputNum = opDescInfo.output_format.size();
    ACL_REQUIRES_POSITIVE(inputNum);
    ACL_REQUIRES_POSITIVE(outputNum);

    *inputDesc = new(std::nothrow) aclTensorDesc[inputNum];
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(*inputDesc);
    *outputDesc = new(std::nothrow) aclTensorDesc[outputNum];
    if (*outputDesc == nullptr) {
        ACL_LOG_INNER_ERROR("[Check][outputDesc]alloc outputDesc memory failed");
        ACL_DELETE_ARRAY_AND_SET_NULL(*inputDesc);
        return ACL_ERROR_FAILURE;
    }

    ACL_REQUIRES_EQ(opDescInfo.input_data_type.size(), inputNum);
    ACL_REQUIRES_EQ(opDescInfo.input_shape.size(), inputNum);
    ACL_REQUIRES_EQ(opDescInfo.input_addrs.size(), inputNum);
    for (size_t idx = 0U; idx < inputNum; ++idx) {
        (*inputDesc)[idx].format = static_cast<aclFormat>(opDescInfo.input_format[idx]);
        (*inputDesc)[idx].dataType = static_cast<aclDataType>(opDescInfo.input_data_type[idx]);
        (*inputDesc)[idx].dims.assign(opDescInfo.input_shape[idx].begin(), opDescInfo.input_shape[idx].end());
        (*inputDesc)[idx].address = opDescInfo.input_addrs[idx];
    }

    ACL_REQUIRES_EQ(opDescInfo.output_data_type.size(), outputNum);
    ACL_REQUIRES_EQ(opDescInfo.output_shape.size(), outputNum);
    ACL_REQUIRES_EQ(opDescInfo.output_addrs.size(), outputNum);
    for (size_t idx = 0U; idx < outputNum; ++idx) {
        (*outputDesc)[idx].format = static_cast<aclFormat>(opDescInfo.output_format[idx]);
        (*outputDesc)[idx].dataType = static_cast<aclDataType>(opDescInfo.output_data_type[idx]);
        (*outputDesc)[idx].dims.assign(opDescInfo.output_shape[idx].begin(), opDescInfo.output_shape[idx].end());
        (*outputDesc)[idx].address = opDescInfo.output_addrs[idx];
    }

    *numInputs = inputNum;
    *numOutputs = outputNum;
    return ACL_SUCCESS;
}

aclError PrepareOm2Tensor(std::vector<gert::Tensor>& tensor, std::vector<gert::Tensor*>& vec,
                          const size_t inputNum, const aclmdlDataset* const dataset,
                          const std::vector<ge::Om2TensorDesc>& tensorDesc,
                          const uint32_t modelId) {
    for (size_t i = 0UL; i < inputNum; ++i) {
        const auto dataBuffer = dataset->blobs[i].dataBuf;
        if (dataBuffer == nullptr) {
            ACL_LOG_ERROR("[Check][dataBuffer]input dataset blobs is null, modelId[%d], index[%zu]", modelId, i);
            return ACL_ERROR_INVALID_PARAM;
        }
        tensor[i].MutableTensorData().SetPlacement(gert::kOnDeviceHbm);
        (void)tensor[i].MutableTensorData().SetAddr(dataBuffer->data, nullptr);
        tensor[i].MutableTensorData().SetSize(dataBuffer->length);

        // Convert std::vector<int64_t> to gert::Shape
        const auto& originShapeDims = tensorDesc[i].GetOriginShape();
        gert::Shape originShape;
        originShape.SetDimNum(originShapeDims.size());
        for (size_t j = 0U; j < originShapeDims.size(); ++j) {
            originShape.SetDim(j, originShapeDims[j]);
        }
        tensor[i].MutableOriginShape() = originShape;

        const auto& storageshapeDims = tensorDesc[i].GetShape();
        gert::Shape storageShape;
        storageShape.SetDimNum(storageshapeDims.size());
        for (size_t j = 0U; j < storageshapeDims.size(); ++j) {
            storageShape.SetDim(j, storageshapeDims[j]);
        }
        tensor[i].MutableStorageShape() = storageShape;

        tensor[i].SetStorageFormat(tensorDesc[i].GetFormat());
        tensor[i].SetOriginFormat(tensorDesc[i].GetOriginFormat());
        tensor[i].SetDataType(tensorDesc[i].GetDataType());
        vec[i] = &(tensor[i]);
    }
    return ACL_SUCCESS;
}

aclError Om2GetModelTensorDesc(std::vector<ge::Om2TensorDesc>& inputDesc, std::vector<ge::Om2TensorDesc>& outputDesc,
                               size_t& inputNum, size_t& outputNum,
                               const std::shared_ptr<gert::Om2ModelExecutor>& executor,
                               const aclmdlDataset* const input, const aclmdlDataset* const output,
                               const uint32_t modelId) {
    ge::Status getDescRet = executor->GetModelDescInfo(inputDesc, outputDesc);
    if (getDescRet != ge::SUCCESS) {
        ACL_LOG_ERROR("[Get][ModelDesc]Get model desc info failed, ge result[%u], modelId[%u]", getDescRet, modelId);
        return ACL_GET_ERRCODE_GE(static_cast<int32_t>(getDescRet));
    }

    inputNum = inputDesc.size();
    outputNum = outputDesc.size();
    ACL_LOG_INFO(
        "Om2GetModelTensorDesc get model input num %zu, output num %zu, input blobs num %zu, output blobs num %zu",
        inputNum, outputNum, input->blobs.size(), output->blobs.size());
    if (input->blobs.size() != inputNum) {
        ACL_LOG_ERROR("Input blobs size mismatch. actual_size[%zu], expected_size[%zu]", input->blobs.size(), inputNum);
        return ACL_ERROR_INVALID_PARAM;
    }

    if (output->blobs.size() != outputNum) {
        ACL_LOG_ERROR("Output blobs size mismatch. actual_size[%zu], expected_size[%zu]", output->blobs.size(),
                      outputNum);
        return ACL_ERROR_INVALID_PARAM;
    }
    return ACL_SUCCESS;
}

aclError Om2UpdateOutputTensorDesc(aclmdlDataset* const & output, std::vector<gert::Tensor>& outputTensor,
                                   const bool isAsync) {
    for (size_t i = 0UL; i < output->blobs.size(); ++i) {
        if (output->blobs[i].tensorDesc != nullptr) {
            aclDestroyTensorDesc(output->blobs[i].tensorDesc);
        }
        output->blobs[i].tensorDesc = aclCreateTensorDesc(ACL_DT_UNDEFINED, 0, nullptr, ACL_FORMAT_UNDEFINED);
    }

    for (size_t i = 0UL; i < outputTensor.size(); ++i) {
        if (output->blobs[i].tensorDesc != nullptr) {
            output->blobs[i].tensorDesc->dims.clear();
            const ge::Format format = outputTensor[i].GetFormat().GetStorageFormat();
            const ge::DataType dataType = outputTensor[i].GetDataType();
            for (size_t idx = 0U; idx < outputTensor[i].MutableStorageShape().GetDimNum(); ++idx) {
                output->blobs[i].tensorDesc->dims.push_back(outputTensor[i].MutableStorageShape().GetDim(idx));
            }
            if (format != ge::FORMAT_RESERVED) {
                output->blobs[i].tensorDesc->format = static_cast<aclFormat>(format);
            }
            if (dataType != ge::DT_UNDEFINED) {
                output->blobs[i].tensorDesc->dataType = static_cast<aclDataType>(dataType);
            }
        }

        auto& dataBuffer = output->blobs[i].dataBuf;
        if ((dataBuffer->data == nullptr) && (!isAsync)) {
            ACL_REQUIRES_NOT_NULL(outputTensor[i].MutableTensorData().GetAddr());
            const auto outputSize = outputTensor[i].MutableTensorData().GetSize();
            ACL_REQUIRES_CALL_RTS_OK(
                aclrtMalloc(reinterpret_cast<void **>(&dataBuffer->data), outputSize, ACL_MEM_TYPE_HIGH_BAND_WIDTH),
                aclrtMalloc);
            ACL_REQUIRES_CALL_RTS_OK(
                aclrtMemcpy(dataBuffer->data, outputSize, outputTensor[i].MutableTensorData().GetAddr(), outputSize,
                    ACL_MEMCPY_DEVICE_TO_DEVICE),
                aclrtMemcpy);
            dataBuffer->length = outputSize;
            ACL_LOG_DEBUG("ModelExecute, assign acl-malloced output addr to user-defined buffer, addr:[%p], len:[%lu]",
                          dataBuffer->data, dataBuffer->length);
        }
    }
    return ACL_SUCCESS;
}

// Internal OM2 model loading functions
aclError ConstructOm2ModelLoadArg(void* workPtr, size_t workSize, void* weightPtr, size_t weightSize,
                                  gert::Om2ModelLoadArg& loadArgs, gert::RtSession* rtSession = nullptr,
                                  const std::vector<ge::FileConstantMem>& fileConstantMems = std::vector<
                                  ge::FileConstantMem>()) {
    loadArgs = {};
    loadArgs.work_ptr = workPtr;
    loadArgs.work_size = workSize;
    loadArgs.weight_ptr = weightPtr;
    loadArgs.weight_size = weightSize;
    loadArgs.rt_session = rtSession;
    loadArgs.file_constant_mems = fileConstantMems;
    return SetOm2ModelLoadArgDevice(loadArgs);
}

aclError Om2ModelLoadFromFileWithMem(const char_t* const modelPath, uint32_t* const modelId,
                                     const gert::Om2ModelLoadArg& loadArgs) {
    auto rtSession = acl::AclResourceManagerOm2::GetInstance().CreateRtSession();
    ACL_REQUIRES_NOT_NULL(rtSession);
    ge::ModelData modelData;
    auto ret = gert::LoadOm2DataFromFile(modelPath, modelData);
    std::shared_ptr<void> dataGuarder;
    dataGuarder.reset(modelData.model_data, [](const void* const p) {
        if (p != nullptr) {
            delete[] static_cast<const uint8_t*>(p);
        }
    });
    if (ret != ge::SUCCESS) {
        ACL_LOG_CALL_ERROR("[Model][FromData]call gert::LoadOm2DataFromFile failed, ge result[%u]", ret);
        return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
    }

    auto loadArgsWithModelId = loadArgs;
    *modelId = acl::AclResourceManagerOm2::GetInstance().GenerateModelId();
    loadArgsWithModelId.model_id = *modelId;
    std::unique_ptr<gert::Om2ModelExecutor> executor =
        gert::LoadOm2ExecutorFromData(modelData, loadArgsWithModelId, ret);
    if (ret != ge::SUCCESS) {
        ACL_LOG_CALL_ERROR("[Model][FromData]call gert::LoadOm2ExecutorFromData failed, ge result[%u]", ret);
        return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
    }
    ACL_REQUIRES_NOT_NULL(executor);
    acl::AclResourceManagerOm2::GetInstance().AddOm2ExecutorWithModelId(*modelId, std::move(executor), rtSession);
    return ACL_SUCCESS;
}

aclError Om2ModelLoadFromMemWithMem(const void* const model, const size_t modelSize,
                                    uint32_t* const modelId, const gert::Om2ModelLoadArg& loadArgs,
                                    const char_t* const weightPath) {
    ge::ModelData modelData = {};
    modelData.model_data = const_cast<void*>(model);
    modelData.model_len = static_cast<uint64_t>(modelSize);
    if (weightPath != nullptr) {
        modelData.weight_path = std::string(weightPath);
        ACL_LOG_INFO("[Model][WeightPath]Load weight path is [%s]", modelData.weight_path.c_str());
    }
    ge::Status errorStatus = ge::SUCCESS;
    auto loadArgsWithModelId = loadArgs;
    *modelId = acl::AclResourceManagerOm2::GetInstance().GenerateModelId();
    loadArgsWithModelId.model_id = *modelId;
    std::unique_ptr<gert::Om2ModelExecutor> executor =
        gert::LoadOm2ExecutorFromData(modelData, loadArgsWithModelId, errorStatus);
    if (errorStatus != ge::SUCCESS) {
        ACL_LOG_CALL_ERROR("[Model][FromData]call gert::LoadOm2ExecutorFromData failed, ge result[%u]", errorStatus);
        return ACL_GET_ERRCODE_GE(static_cast<int32_t>(errorStatus));
    }
    ACL_REQUIRES_NOT_NULL(executor);
    acl::AclResourceManagerOm2::GetInstance().AddOm2ExecutorWithModelId(*modelId, std::move(executor), nullptr);
    return ACL_SUCCESS;
}

// FillModelDescFromTensorDescs - internal helper for populating aclmdlDesc
aclError FillModelDescFromTensorDescs(aclmdlDesc* modelDesc,
                                      const std::vector<ge::Om2TensorDesc>& inputDesc,
                                      const std::vector<ge::Om2TensorDesc>& outputDesc,
                                      const std::vector<ge::Om2TensorDesc>& inputDescV2,
                                      const std::vector<ge::Om2TensorDesc>& outputDescV2) {
    // Check size consistency
    if ((inputDescV2.size() < inputDesc.size()) || (outputDescV2.size() < outputDesc.size())) {
        ACL_LOG_CALL_ERROR("[Get][ModelDescInfo]description v2 size less than description v1 size");
        return ACL_ERROR_FAILURE;
    }

    // Fill input descriptions
    for (size_t i = 0U; i < inputDesc.size(); ++i) {
        aclmdlTensorDesc tensorDesc;
        tensorDesc.size = inputDesc[i].GetSize();
        tensorDesc.name = inputDesc[i].GetName();
        tensorDesc.format = static_cast<aclFormat>(inputDesc[i].GetFormat());
        tensorDesc.dataType = static_cast<aclDataType>(inputDesc[i].GetDataType());
        tensorDesc.dims = inputDesc[i].GetShape();
        tensorDesc.dimsV2 = inputDescV2[i].GetShape();
        tensorDesc.shapeRanges = inputDesc[i].GetShapeRange();
        modelDesc->inputDesc.push_back(tensorDesc);
    }

    // Fill output descriptions
    for (size_t i = 0U; i < outputDesc.size(); ++i) {
        aclmdlTensorDesc tensorDesc;
        tensorDesc.size = outputDesc[i].GetSize();
        tensorDesc.name = outputDesc[i].GetName();
        tensorDesc.format = static_cast<aclFormat>(outputDesc[i].GetFormat());
        tensorDesc.dataType = static_cast<aclDataType>(outputDesc[i].GetDataType());
        tensorDesc.dims = outputDesc[i].GetShape();
        tensorDesc.dimsV2 = outputDescV2[i].GetShape();
        tensorDesc.shapeRanges = outputDesc[i].GetShapeRange();
        modelDesc->outputDesc.push_back(tensorDesc);
    }

    return ACL_SUCCESS;
}

// PopulateDescFromOm2Data - internal helper for populating aclmdlDesc from OM2 data
aclError PopulateDescFromOm2Data(aclmdlDesc* modelDesc, const ge::ModelData& om2Data) {
    ACL_LOG_INFO("Populating aclmdlDesc from OM2 data");

    // Create temporary executor from OM2 data
    gert::Om2ModelLoadArg loadArgs;
    ge::graphStatus ret = ge::GRAPH_SUCCESS;
    // Remove const qualifier for LoadOm2ExecutorFromData call
    ge::ModelData& mutableOm2Data = const_cast<ge::ModelData&>(om2Data);
    std::unique_ptr<gert::Om2ModelExecutor> executor = gert::LoadOm2ExecutorFromData(mutableOm2Data, loadArgs, ret);
    if (executor == nullptr) {
        ACL_LOG_CALL_ERROR("[Load][Om2Executor]create temporary OM2 executor failed, ge result[%u]", ret);
        return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
    }

    // Get model desc info (v1)
    std::vector<ge::Om2TensorDesc> inputDesc;
    std::vector<ge::Om2TensorDesc> outputDesc;
    ret = executor->GetModelDescInfo(inputDesc, outputDesc);
    if (ret != ge::SUCCESS) {
        ACL_LOG_CALL_ERROR("[Get][ModelDescInfo]get om2 model description failed, ge result[%u]", ret);
        return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
    }

    // Get model desc info (v2)
    std::vector<ge::Om2TensorDesc> inputDescV2;
    std::vector<ge::Om2TensorDesc> outputDescV2;
    ret = executor->GetModelDescInfo(inputDescV2, outputDescV2, true);
    if (ret != ge::SUCCESS) {
        ACL_LOG_CALL_ERROR("[Get][ModelDescInfo]get om2 model description v2 failed, ge result[%u]", ret);
        return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
    }

    // Fill aclmdlDesc using common helper
    ACL_REQUIRES_OK(FillModelDescFromTensorDescs(modelDesc, inputDesc, outputDesc, inputDescV2, outputDescV2));

    ACL_LOG_INFO("Successfully populated aclmdlDesc from OM2 data");
    return ACL_SUCCESS;
}

// Om2ModelExecuteCommon - internal function for executing OM2 models
aclError Om2ModelExecuteCommon(uint32_t modelId, const aclmdlDataset* input, aclmdlDataset* output,
                               bool isAsync, aclrtStream stream) {
    ACL_LOG_INFO("start to execute Om2ModelExecuteCommon, modelId[%u]", modelId);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(input);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(output);

    auto const executor = acl::AclResourceManagerOm2::GetInstance().GetOm2Executor(modelId);
    if (executor == nullptr) {
        ACL_LOG_ERROR("input modelId[%u] is invalid, please make sure model has been loaded", modelId);
        return static_cast<aclError>(ACL_ERROR_GE_EXEC_MODEL_ID_INVALID);
    }

    // Get model description info
    std::vector<ge::Om2TensorDesc> inputDesc;
    std::vector<ge::Om2TensorDesc> outputDesc;
    size_t inputNum = 0;
    size_t outputNum = 0;
    auto ret = Om2GetModelTensorDesc(inputDesc, outputDesc, inputNum, outputNum, executor, input, output, modelId);
    if (ret != ACL_SUCCESS) {
        ACL_LOG_ERROR("Get model TensorDesc failed.");
        return ret;
    }

    // Prepare input tensors
    std::vector<gert::Tensor> inputTensor(inputNum);
    std::vector<gert::Tensor*> inputVec(inputNum);
    ret = PrepareOm2Tensor(inputTensor, inputVec, inputNum, input, inputDesc, modelId);
    if (ret != ACL_SUCCESS) {
        ACL_LOG_ERROR("Prepare input tensors failed.");
        return ret;
    }

    // Prepare output tensors
    std::vector<gert::Tensor> outputTensor(outputNum);
    std::vector<gert::Tensor*> outputVec(outputNum);
    ret = PrepareOm2Tensor(outputTensor, outputVec, outputNum, output, outputDesc, modelId);
    if (ret != ACL_SUCCESS) {
        ACL_LOG_ERROR("Prepare output tensors failed.");
        return ret;
    }

    // Call executor run with tensor pointers
    ge::Status run_ret = ge::GRAPH_SUCCESS;
    if (isAsync) {
        run_ret = executor->RunAsync(stream, inputVec, outputVec);
    } else {
        run_ret = executor->Run(inputVec, outputVec);
    }
    if (run_ret != ge::SUCCESS) {
        ACL_LOG_CALL_ERROR("[Exec][Model]Execute model failed, ge result[%u], modelId[%u]", run_ret, modelId);
        return ACL_GET_ERRCODE_GE(static_cast<int32_t>(run_ret));
    }

    // Update output tensor descriptions
    ret = Om2UpdateOutputTensorDesc(output, outputTensor, isAsync);
    if (ret != ACL_SUCCESS) {
        ACL_LOG_ERROR("Update output tensors descriptions failed.");
        return ret;
    }

    ACL_LOG_INFO("successfully execute Om2ModelExecuteCommon, modelId[%u]", modelId);
    return ACL_SUCCESS;
}
} // namespace

namespace acl {
static size_t aclmdlGetTensorSize(aclmdlTensorDesc& tensorDesc, size_t idx) {
    std::vector<int64_t>& dims = tensorDesc.dims;
    bool needCalcByMaxShapeRange = false;
    for (size_t i = 0U; i < dims.size(); ++i) {
        if (dims[i] < 0) {
            needCalcByMaxShapeRange = true;
            break;
        }
    }

    if (tensorDesc.shapeRanges.empty()) {
        needCalcByMaxShapeRange = false;
    }

    if (needCalcByMaxShapeRange) {
        std::vector<std::pair<int64_t, int64_t>>& shapeRanges = tensorDesc.shapeRanges;
        const size_t elementTypeSize = aclDataTypeSize(tensorDesc.dataType);
        size_t outputSizeByMaxShapeRange = elementTypeSize;
        for (size_t i = 0U; i < shapeRanges.size(); ++i) {
            if (shapeRanges[i].second <= 0) {
                ACL_LOG_INFO("max shape of shapeRanges[%zu] is [%ld], index[%zu]",
                             i, shapeRanges[i].second, idx);
                return 0U;
            }
            if (acl::CheckSizeTMultiOverflow(outputSizeByMaxShapeRange, static_cast<size_t>(shapeRanges[i].second),
                                             outputSizeByMaxShapeRange) == ACL_ERROR_FAILURE) {
                return 0U;
            }
        }
        return outputSizeByMaxShapeRange;
    }

    return tensorDesc.size;
}

// Constants for tensor name conversion
constexpr const char_t* TENSOR_NAME_PREFIX = "acl";
constexpr const char_t* TENSOR_INPUT_STR = "input";
constexpr const char_t* TENSOR_OUTPUT_STR = "output";
constexpr const char_t* MODEL_ID_STR = "modelId";
constexpr size_t TENSOR_NAME_ATTR_NUM = 5U;

// get real tensor name from modelDesc, it will return nullptr if tensorName isn't in modelDesc
static const char_t* GetRealTensorName(const aclmdlDesc* const modelDesc, const std::string& tensorName) {
    for (size_t idx = 0U; idx < modelDesc->inputDesc.size(); ++idx) {
        if (modelDesc->inputDesc[idx].name == tensorName) {
            return modelDesc->inputDesc[idx].name.c_str();
        }
    }

    for (size_t idx = 0U; idx < modelDesc->outputDesc.size(); ++idx) {
        if (modelDesc->outputDesc[idx].name == tensorName) {
            return modelDesc->outputDesc[idx].name.c_str();
        }
    }
    return nullptr;
}

static bool IsConvertTensorNameLegal(const aclmdlDesc* const modelDesc, const std::string& tensorName) {
    return (GetRealTensorName(modelDesc, tensorName) == nullptr);
}

// current conversion tensor name illegal needs to be transformed
static bool TransConvertTensorNameToLegal(const aclmdlDesc* const modelDesc, std::string& tensorName) {
    size_t depth = 0U;
    tensorName = tensorName + "_";
    std::queue<std::string> q;
    q.push(tensorName);
    constexpr size_t maxDepth = 3U;
    while (!q.empty()) {
        if (depth == maxDepth) {
            ACL_LOG_INFO("reach max depth[%zu], cannot generate legal convert tensor name", maxDepth);
            tensorName = tensorName.substr(0U, tensorName.size() - 1U);
            return false;
        }
        const size_t len = q.size();
        for (size_t idx = 0U; idx < len; ++idx) {
            std::string curTensorName = q.front();
            q.pop();
            for (char_t c = 'a'; c <= 'z'; ++c) {
                curTensorName += c;
                if (IsConvertTensorNameLegal(modelDesc, curTensorName)) {
                    tensorName = curTensorName;
                    return true;
                }
                q.push(curTensorName);
                curTensorName = curTensorName.substr(0U, curTensorName.size() - 1U);
            }
        }
        depth++;
    }
    return false;
}

// convert params to convertName
static void GetConvertTensorName(const aclmdlDesc* const modelDesc, const size_t idx,
                                 const TensorType tensorType, std::string& convertName) {
    convertName = std::string(TENSOR_NAME_PREFIX) + "_" +
        std::string(MODEL_ID_STR) + "_" + std::to_string(modelDesc->modelId);
    if (tensorType == TensorType::INPUT_TENSOR_TYPE) {
        convertName += ("_" + std::string(TENSOR_INPUT_STR));
    } else {
        convertName += ("_" + std::string(TENSOR_OUTPUT_STR));
    }
    convertName += ("_" + std::to_string(idx));
    ACL_LOG_INFO("convert realname of tensor success, conversion name = %s", convertName.c_str());
}

// get tensor name to dims with or without realname
static aclError GetTensorDescNameToDims(const aclmdlDesc* const modelDesc, const std::string& realName,
                                        const TensorType tensorType, const size_t idx, aclmdlIODims* const dims) {
    const size_t dimsNameLen = sizeof(dims->name);
    std::string tensorName;
    if ((realName.size() + 1U) > dimsNameLen) {
        // use conversion name because realname is too long
        ACL_LOG_INFO("use conversion name because real tensor name is over than %zu", dimsNameLen);
        GetConvertTensorName(modelDesc, idx, tensorType, tensorName);
        if (!IsConvertTensorNameLegal(modelDesc, tensorName)) {
            if (!TransConvertTensorNameToLegal(modelDesc, tensorName)) {
                ACL_LOG_WARN("cannot generate legal tensor name, use conversion name %s may has conflict risk",
                             tensorName.c_str());
            }
        }
    } else {
        tensorName = realName;
    }

    const auto ret = strncpy_s(dims->name, dimsNameLen, tensorName.c_str(), tensorName.size());
    if (ret != EOK) {
        ACL_LOG_INNER_ERROR("[Copy][Str]call strncpy_s failed, result = %d", ret);
        return ACL_ERROR_FAILURE;
    }
    return ACL_SUCCESS;
}

static aclError GetDims(const aclmdlDesc* const modelDesc, const TensorType tensorType, const DimsType dimsType,
                        const size_t idx, aclmdlIODims* const dims) {
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

    const aclmdlTensorDesc& tensorDesc = desc[idx];
    const auto ret = GetTensorDescNameToDims(modelDesc, tensorDesc.name, tensorType, idx, dims);
    if (ret != ACL_SUCCESS) {
        ACL_LOG_INNER_ERROR("[Get][TensorDescName]get tensor desc name to dims failed, errorCode = %d", ret);
        return ret;
    }
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

    return ACL_SUCCESS;
}

static aclError GetDimsRange(const aclmdlDesc* const modelDesc, const TensorType tensorType, const size_t idx,
                             aclmdlIODimsRange* const dimsRange) {
    ACL_REQUIRES_NOT_NULL(dimsRange);
    const bool isDynamicGear = (!modelDesc->dynamicBatch.empty() || !modelDesc->dynamicDims.empty() ||
        !modelDesc->dynamicHW.empty());
    if (isDynamicGear) {
        dimsRange->rangeCount = 0U;
        ACL_LOG_INFO("set rangeCount[%zu] in the dynamic gear model scenario success.", dimsRange->rangeCount);
        return ACL_SUCCESS;
    }

    std::vector<aclmdlTensorDesc> desc;
    if (tensorType == TensorType::INPUT_TENSOR_TYPE) {
        desc = modelDesc->inputDesc;
    } else {
        desc = modelDesc->outputDesc;
    }

    const size_t descSize = desc.size();
    if (idx >= descSize) {
        ACL_LOG_INNER_ERROR("[Check][Params]GetDimsRange failed, index[%zu] can not greater than or equal to tensor "
                            "size[%zu]", idx, descSize);
        return ACL_ERROR_INVALID_PARAM;
    }

    const auto& tensorDesc = desc[idx];
    const auto& shapeRanges = tensorDesc.shapeRanges;
    const size_t dynamicDimSize = shapeRanges.size();
    constexpr size_t MIN = 0U;
    constexpr size_t MAX = 1U;
    if (dynamicDimSize > static_cast<size_t>(ACL_MAX_DIM_CNT)) {
        ACL_LOG_INNER_ERROR("[Check][dynamicDimSize]GetDimsRange failed, dim size[%zu] can not larger than max[%d]",
                            dynamicDimSize, ACL_MAX_DIM_CNT);
        return ACL_ERROR_INTERNAL_ERROR;
    }
    if (dynamicDimSize == 0U) {
        const auto& staticDims = tensorDesc.dims;
        const size_t staticDimSize = staticDims.size();
        if (staticDimSize > static_cast<size_t>(ACL_MAX_DIM_CNT)) {
            ACL_LOG_INNER_ERROR("[Check][staticDimSize]GetDimsRange failed, dim size[%zu] can not larger than max[%d]",
                                staticDimSize, ACL_MAX_DIM_CNT);
            return ACL_ERROR_INTERNAL_ERROR;
        }
        dimsRange->rangeCount = staticDimSize;
        ACL_LOG_INFO("set rangeCount[%zu] in the static model scenario success.", dimsRange->rangeCount);
        for (size_t i = 0U; i < staticDimSize; ++i) {
            dimsRange->range[i][MIN] = staticDims[i];
            dimsRange->range[i][MAX] = staticDims[i];
            ACL_LOG_INFO("set range[%zu][%zu] to [%ld] and range[%zu][%zu] to [%ld] success.", i, MIN,
                         dimsRange->range[i][MIN], i, MAX, dimsRange->range[i][MAX]);
        }
    } else {
        if (dynamicDimSize != tensorDesc.dims.size()) {
            ACL_LOG_INNER_ERROR("[Check][dynamicDimSize]GetDimsRange failed, size of shapeRanges[%zu] does not equal "
                                "to size of dims[%zu]", dynamicDimSize, tensorDesc.dims.size());
            return ACL_ERROR_INTERNAL_ERROR;
        }
        dimsRange->rangeCount = dynamicDimSize;
        ACL_LOG_INFO("set rangeCount[%zu] in the dynamic model scenario success.", dimsRange->rangeCount);
        for (size_t i = 0U; i < dynamicDimSize; ++i) {
            dimsRange->range[i][MIN] = shapeRanges[i].first;
            dimsRange->range[i][MAX] = shapeRanges[i].second;
            ACL_LOG_INFO("set range[%zu][%zu] to [%ld] and range[%zu][%zu] to [%ld] success.", i, MIN,
                         dimsRange->range[i][MIN], i, MAX, dimsRange->range[i][MAX]);
        }
    }

    return ACL_SUCCESS;
}

static const char* aclmdlGetNameByIndex(const std::vector<aclmdlTensorDesc>& desc, const size_t idx) {
    if (idx >= desc.size()) {
        ACL_LOG_ERROR("[Check][index]get name by index failed, index[%zu] is larger than or equal to desc size[%zu]",
                      idx, desc.size());
        const std::string errMsg = acl::AclErrorLogManager::FormatStr("cannot larger than or equal to desc size[%zu]",
                                                                      desc.size());
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
                                                  std::vector<const char*>({"param", "value", "reason"}),
                                                  std::vector<const char*>({
                                                      "index", std::to_string(idx).c_str(), errMsg.c_str()
                                                  }));
        return "";
    }

    return desc[idx].name.c_str();
}

static aclFormat aclmdlGetFormat(const std::vector<aclmdlTensorDesc>& desc, const size_t idx) {
    if (idx >= desc.size()) {
        ACL_LOG_INNER_ERROR("[Check][index]get data format by index failed, index[%zu] is larger "
                            "than or equal to desc size[%zu]", idx, desc.size());
        return ACL_FORMAT_UNDEFINED;
    }

    return desc[idx].format;
}

static aclDataType aclmdlGetDataType(const std::vector<aclmdlTensorDesc>& desc, const size_t idx) {
    if (idx >= desc.size()) {
        ACL_LOG_INNER_ERROR("[Check][Index]get data type by index failed, index[%zu] is larger than or "
                            "equal to desc size[%zu]", idx, desc.size());
        return ACL_DT_UNDEFINED;
    }

    return desc[idx].dataType;
}

static aclError aclmdlGetIndexByName(const std::vector<aclmdlTensorDesc>& desc,
                                     const char_t* const name, size_t* const idx) {
    ACL_REQUIRES_NOT_NULL(name);
    ACL_REQUIRES_NOT_NULL(idx);

    const std::string tensorName(name);
    for (size_t i = 0U; i < desc.size(); ++i) {
        if (desc[i].name == tensorName) {
            *idx = i;
            ACL_LOG_DEBUG("success to get tensor[%s] index[%zu]", name, *idx);
            return ACL_SUCCESS;
        }
    }

    ACL_LOG_INNER_ERROR("[Get][Index]get index by name failed, cannot find tensor name[%s]", name);
    return ACL_ERROR_INVALID_PARAM;
}

// try to transfer conversion name to real tensor name, it will return nullptr if conversion name isn't
// satisfy condition
static const char_t* TransTensorNameToReal(const aclmdlDesc* const modelDesc, const std::string& tensorName) {
    std::vector<std::string> valArr;
    acl::StringUtils::Split(tensorName, '_', valArr);
    if ((valArr.size() != TENSOR_NAME_ATTR_NUM) && (valArr.size() != (TENSOR_NAME_ATTR_NUM + 1U))) {
        ACL_LOG_INNER_ERROR("[Check][Params]tensorName[%s] cannot be devided into %zu parts",
                            tensorName.c_str(), TENSOR_NAME_ATTR_NUM);
        return nullptr;
    }
    if (valArr[0U] != TENSOR_NAME_PREFIX) {
        ACL_LOG_INNER_ERROR("[Check][Param]cannot find Attr[%s] in tensorName[%s]",
                            TENSOR_NAME_PREFIX, tensorName.c_str());
        return nullptr;
    }
    const int32_t base = 10;
    if ((valArr[1U] == MODEL_ID_STR) && (acl::StringUtils::IsDigit(valArr[2U]))) {
        const auto modelId = strtoul(valArr[2U].c_str(), nullptr, base);
        if (modelId != modelDesc->modelId) {
            ACL_LOG_INNER_ERROR("[Check][modelId]modelId[%lu] is invalid, tensorName[%s]", modelId, tensorName.c_str());
            return nullptr;
        }
    } else {
        ACL_LOG_INNER_ERROR("[Check][modelId]cannot find attr[%s] or modelId in tensorName[%s]",
                            MODEL_ID_STR, tensorName.c_str());
        return nullptr;
    }
    if (acl::StringUtils::IsDigit(valArr[4U])) {
        const auto idex = strtoul(valArr[4U].c_str(), nullptr, base);
        if (valArr[3U] == TENSOR_INPUT_STR) {
            if (idex >= modelDesc->inputDesc.size()) {
                ACL_LOG_INNER_ERROR("[Check][index]inputDesc index[%lu] should be in [0, %zu), tensorName[%s]",
                                    idex, modelDesc->inputDesc.size(), tensorName.c_str());
                return nullptr;
            }
            return modelDesc->inputDesc[idex].name.c_str();
        }
        if (valArr[3U] == TENSOR_OUTPUT_STR) {
            if (idex >= modelDesc->outputDesc.size()) {
                ACL_LOG_INNER_ERROR("[Check][index]outputDesc index[%lu] should be in [0, %zu), tensorName[%s]", idex,
                                    modelDesc->outputDesc.size(), tensorName.c_str());
                return nullptr;
            }
            return modelDesc->outputDesc[idex].name.c_str();
        }
    }

    ACL_LOG_INNER_ERROR("[Find][Attr]cannot find [input_%s] or [output_%s] in tensorName[%s]", valArr[4U].c_str(),
                        valArr[4U].c_str(), tensorName.c_str());
    return nullptr;
}

static aclError LoadFromFile(const aclmdlConfigHandle* handle, const std::vector<ge::FileConstantMem>& fileConstantMems,
                             uint32_t* const modelId) {
    ACL_REQUIRES_OK(CheckOm2UserLoadConfigOptValid(handle));
    gert::Om2ModelLoadArg loadArgs;
    ACL_REQUIRES_OK(ConstructOm2ModelLoadArg(nullptr, 0U, nullptr, 0U, loadArgs, nullptr, fileConstantMems));
    return Om2ModelLoadFromFileWithMem(handle->loadPath.c_str(), modelId, loadArgs);
}

static aclError LoadFromFileWithMem(const aclmdlConfigHandle* handle,
                                    const std::vector<ge::FileConstantMem>& fileConstantMems,
                                    uint32_t* const modelId) {
    ACL_REQUIRES_OK(CheckOm2UserLoadConfigOptValid(handle));
    gert::Om2ModelLoadArg loadArgs;
    ACL_REQUIRES_OK(ConstructOm2ModelLoadArg(handle->workPtr, handle->workSize, handle->weightPtr,
        handle->weightSize, loadArgs, nullptr, fileConstantMems));
    return Om2ModelLoadFromFileWithMem(handle->loadPath.c_str(), modelId, loadArgs);
}

static aclError LoadFromMem(const aclmdlConfigHandle* handle, const std::vector<ge::FileConstantMem>& fileConstantMems,
                            uint32_t* const modelId) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(handle->mdlAddr);
    ACL_REQUIRES_OK(CheckOm2UserLoadConfigOptValid(handle));
    gert::Om2ModelLoadArg loadArgs;
    ACL_REQUIRES_OK(ConstructOm2ModelLoadArg(nullptr, 0U, nullptr, 0U, loadArgs, nullptr, fileConstantMems));
    return Om2ModelLoadFromMemWithMem(handle->mdlAddr, handle->mdlSize, modelId, loadArgs,
                                      handle->weightPath.c_str());
}

static aclError LoadFromMemWithMem(const aclmdlConfigHandle* handle,
                                   const std::vector<ge::FileConstantMem>& fileConstantMems,
                                   uint32_t* const modelId) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(handle->mdlAddr);

    ACL_REQUIRES_OK(CheckOm2UserLoadConfigOptValid(handle));
    gert::Om2ModelLoadArg loadArgs;
    ACL_REQUIRES_OK(ConstructOm2ModelLoadArg(handle->workPtr, handle->workSize, handle->weightPtr,
        handle->weightSize, loadArgs, nullptr, fileConstantMems));
    return Om2ModelLoadFromMemWithMem(handle->mdlAddr, handle->mdlSize, modelId, loadArgs, nullptr);
}

} // namespace acl

aclmdlDesc* aclmdlCreateDescImplOm2() {
    return new(std::nothrow) aclmdlDesc();
}

aclError aclmdlDestroyDescImplOm2(aclmdlDesc* modelDesc) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_DELETE_AND_SET_NULL(modelDesc);
    return ACL_SUCCESS;
}

aclError aclmdlGetDescImplOm2(aclmdlDesc* modelDesc, uint32_t modelId) {
    ACL_PROFILING_REG(acl::AclProfType::AclmdlGetDesc);
    ACL_LOG_INFO("[OM2] start to execute aclmdlGetDesc, modelId[%u]", modelId);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    modelDesc->Clear();

    // Get OM2 executor from resource manager
    auto& rm = acl::AclResourceManagerOm2::GetInstance();
    auto executor = rm.GetOm2Executor(modelId);
    ACL_REQUIRES_NOT_NULL(executor);

    // Get model desc info (v1)
    std::vector<ge::Om2TensorDesc> inputDesc;
    std::vector<ge::Om2TensorDesc> outputDesc;
    ge::Status ret = executor->GetModelDescInfo(inputDesc, outputDesc);
    if (ret != ge::SUCCESS) {
        ACL_LOG_CALL_ERROR("[Get][ModelDescInfo]get om2 model description failed, ge result[%u], modelId[%u]",
                           ret, modelId);
        return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
    }

    // Get model desc info (v2)
    std::vector<ge::Om2TensorDesc> inputDescV2;
    std::vector<ge::Om2TensorDesc> outputDescV2;
    ret = executor->GetModelDescInfo(inputDescV2, outputDescV2, true);
    if (ret != ge::SUCCESS) {
        ACL_LOG_CALL_ERROR("[Get][ModelDescInfo]get om2 model description v2 failed, ge result[%u], modelId[%u]",
                           ret, modelId);
        return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
    }

    // Fill aclmdlDesc using common helper
    ACL_REQUIRES_OK(FillModelDescFromTensorDescs(modelDesc, inputDesc, outputDesc, inputDescV2, outputDescV2));

    modelDesc->modelId = modelId;
    ACL_LOG_INFO("[OM2] successfully execute aclmdlGetDesc, modelId[%u]", modelId);
    return ACL_SUCCESS;
}

aclError aclmdlGetDescFromFileImplOm2(aclmdlDesc* modelDesc, const char* modelPath) {
    ACL_PROFILING_REG(acl::AclProfType::AclmdlGetDesc);
    ACL_LOG_INFO("[OM2] start to execute aclmdlGetDescFromFile, path[%s]", modelPath);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelPath);
    modelDesc->Clear();

    // Load OM2 package from file
    ge::ModelData om2Data;
    ge::Status ret = gert::LoadOm2DataFromFile(std::string(modelPath), om2Data);
    if (ret != ge::SUCCESS) {
        ACL_LOG_CALL_ERROR("[Load][Om2File]load OM2 from file[%s] failed, ge result[%u]", modelPath, ret);
        return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
    }

    // Create shared_ptr guard to manage memory lifecycle
    std::shared_ptr<void> dataGuarder(om2Data.model_data, [](const void* const p) {
        if (p != nullptr) {
            delete[] static_cast<const uint8_t*>(p);
        }
    });

    // Populate desc from OM2 data
    ACL_REQUIRES_OK(PopulateDescFromOm2Data(modelDesc, om2Data));

    ACL_LOG_INFO("[OM2] successfully execute aclmdlGetDescFromFile, path[%s]", modelPath);
    return ACL_SUCCESS;
}

aclError aclmdlGetDescFromMemImplOm2(aclmdlDesc* modelDesc, const void* model, size_t modelSize) {
    ACL_PROFILING_REG(acl::AclProfType::AclmdlGetDesc);
    ACL_LOG_INFO("[OM2] start to execute aclmdlGetDescFromMem, modelSize[%zu]", modelSize);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(model);
    ACL_REQUIRES_POSITIVE_WITH_INPUT_REPORT(modelSize);
    modelDesc->Clear();

    // Create OM2 model data
    ge::ModelData om2Data;
    om2Data.model_data = const_cast<void*>(model);
    om2Data.model_len = static_cast<uint64_t>(modelSize);

    // Populate desc from OM2 data
    ACL_REQUIRES_OK(PopulateDescFromOm2Data(modelDesc, om2Data));

    ACL_LOG_INFO("[OM2] successfully execute aclmdlGetDescFromMem, modelSize[%zu]", modelSize);
    return ACL_SUCCESS;
}

size_t aclmdlGetNumInputsImplOm2(aclmdlDesc* modelDesc) {
    if (modelDesc == nullptr) {
        return 0U;
    }

    return modelDesc->inputDesc.size();
}

size_t aclmdlGetNumOutputsImplOm2(aclmdlDesc* modelDesc) {
    if (modelDesc == nullptr) {
        return 0U;
    }

    return modelDesc->outputDesc.size();
}

size_t aclmdlGetInputSizeByIndexImplOm2(aclmdlDesc* modelDesc, size_t index) {
    if ((modelDesc == nullptr) || (index >= modelDesc->inputDesc.size())) {
        ACL_LOG_INNER_ERROR("input param is invalid, modelDesc[%p], index[%zu]", modelDesc, index);
        return 0U;
    }

    aclmdlTensorDesc& tensorDesc = modelDesc->inputDesc[index];
    return acl::aclmdlGetTensorSize(tensorDesc, index);
}

size_t aclmdlGetOutputSizeByIndexImplOm2(aclmdlDesc* modelDesc, size_t index) {
    if ((modelDesc == nullptr) || (index >= modelDesc->outputDesc.size())) {
        ACL_LOG_INNER_ERROR("input param is invalid, index[%zu]", index);
        return 0U;
    }
    aclmdlTensorDesc& tensorDesc = modelDesc->outputDesc[index];
    return acl::aclmdlGetTensorSize(tensorDesc, index);
}

aclmdlExecConfigHandle* aclmdlCreateExecConfigHandleImplOm2() {
    return new(std::nothrow) aclmdlExecConfigHandle();
}

aclError aclmdlDestroyExecConfigHandleImplOm2(const aclmdlExecConfigHandle* handle) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(handle);
    ACL_DELETE_AND_SET_NULL(handle);
    return ACL_SUCCESS;
}

aclmdlDataset* aclmdlCreateDatasetImplOm2() {
    return new(std::nothrow) aclmdlDataset();
}

aclError aclmdlDestroyDatasetImplOm2(const aclmdlDataset* dataset) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dataset);
    for (size_t i = 0U; i < dataset->blobs.size(); ++i) {
        ACL_DELETE_ARRAY_AND_SET_NULL((const_cast<aclmdlDataset *>(dataset))->blobs[i].tensorDesc);
    }
    ACL_DELETE_AND_SET_NULL(dataset);
    return ACL_SUCCESS;
}

aclError aclmdlAddDatasetBufferImplOm2(aclmdlDataset* dataset, aclDataBuffer* dataBuffer) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dataset);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dataBuffer);
    const acl::AclModelTensor tensor = acl::AclModelTensor(dataBuffer, nullptr);
    dataset->blobs.push_back(tensor);
    return ACL_SUCCESS;
}

size_t aclmdlGetDatasetNumBuffersImplOm2(const aclmdlDataset* dataset) {
    if (dataset == nullptr) {
        ACL_LOG_ERROR("[Check][Dataset]input param[dataset] is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
                                                  std::vector<const char*>({"param"}),
                                                  std::vector<const char*>({"dataset"}));
        return 0U;
    }

    return dataset->blobs.size();
}

aclDataBuffer* aclmdlGetDatasetBufferImplOm2(const aclmdlDataset* dataset, size_t index) {
    if ((dataset == nullptr) || (index >= dataset->blobs.size())) {
        ACL_LOG_ERROR("[Check][Params]input param is invalid, dataset[%p], index[%zu]", dataset, index);
        const std::string errMsg = acl::AclErrorLogManager::FormatStr("dataset[%p], index[%zu]", dataset, index);
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
                                                  std::vector<const char*>({"param", "value", "reason"}),
                                                  std::vector<const char*>({
                                                      "input param", errMsg.c_str(), "check failed"
                                                  }));
        return nullptr;
    }

    return dataset->blobs[index].dataBuf;
}

aclTensorDesc* aclmdlGetDatasetTensorDescImplOm2(const aclmdlDataset* dataset, size_t index) {
    ACL_REQUIRES_NOT_NULL_RET_NULL_INPUT_REPORT(dataset);
    if (index >= dataset->blobs.size()) {
        ACL_LOG_ERROR("[Check][Index]input param index[%zu] must be smaller than output databuf size[%zu]",
                      index, dataset->blobs.size());
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
                                                  std::vector<const char*>({"param", "value", "reason"}),
                                                  std::vector<const char*>({
                                                      "index", std::to_string(index).c_str(),
                                                      "must be smaller than output databuf size"
                                                  }));
        return nullptr;
    }
    return dataset->blobs[index].tensorDesc;
}

aclError aclmdlSetDatasetTensorDescImplOm2(aclmdlDataset* dataset, aclTensorDesc* tensorDesc, size_t index) {
    ACL_REQUIRES_NOT_NULL(dataset);
    if (index >= dataset->blobs.size()) {
        ACL_LOG_ERROR("[Check][Index]input param index[%zu] must be smaller than input databuf size[%zu]",
                      index, dataset->blobs.size());
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
                                                  std::vector<const char*>({"param", "value", "reason"}),
                                                  std::vector<const char*>({
                                                      "index", std::to_string(index).c_str(),
                                                      "must be smaller than input databuf size"
                                                  }));
        return ACL_ERROR_INVALID_PARAM;
    }

    if (tensorDesc == nullptr) {
        ACL_DELETE_AND_SET_NULL(dataset->blobs[index].tensorDesc);
        ACL_LOG_INFO("Set tensorDesc for tensor[%zu] successfully, tensorDesc is nullptr", index);
        return ACL_SUCCESS;
    }

    if (dataset->blobs[index].tensorDesc == nullptr) {
        dataset->blobs[index].tensorDesc = new(std::nothrow) aclTensorDesc[1]{*tensorDesc};
        ACL_CHECK_MALLOC_RESULT(dataset->blobs[index].tensorDesc);
    } else {
        *(dataset->blobs[index].tensorDesc) = *tensorDesc;
    }
    ACL_LOG_INFO("Set tensorDesc %s for tensor[%zu] successfully", tensorDesc->DebugString().c_str(), index);
    return ACL_SUCCESS;
}

aclError aclmdlBundleLoadFromFileImplOm2(const char* modelPath, uint32_t* bundleId) {
    (void)modelPath;
    (void)bundleId;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlBundleLoadFromMemImplOm2(const void* model, size_t modelSize, uint32_t* bundleId) {
    (void)model;
    (void)modelSize;
    (void)bundleId;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlLoadFromFileImplOm2(const char* modelPath, uint32_t* modelId) {
    ACL_PROFILING_REG(acl::AclProfType::AclmdlLoadFromFile);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelPath);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelId);

    gert::Om2ModelLoadArg loadArgs;
    ACL_REQUIRES_OK(ConstructOm2ModelLoadArg(nullptr, 0U, nullptr, 0U, loadArgs));

    const aclError ret = Om2ModelLoadFromFileWithMem(modelPath, modelId, loadArgs);
    if (ret != ACL_SUCCESS) {
        ACL_LOG_ERROR("[OM2] Load OM2 model from file failed, path [%s], errorCode [%u]", modelPath, ret);
    } else {
        ACL_LOG_INFO("[OM2] Successfully execute aclmdlLoadFromFile, modelId[%u], modelPath[%s]", *modelId, modelPath);
    }
    return ret;
}

aclError aclmdlLoadFromFileWithMemImplOm2(const char* modelPath, uint32_t* modelId,
                                          void* workPtr, size_t workSize,
                                          void* weightPtr, size_t weightSize) {
    ACL_PROFILING_REG(acl::AclProfType::AclmdlLoadFromFileWithMem);
    ACL_LOG_INFO("[OM2] start to execute aclmdlLoadFromFileWithMem, workSize[%zu], weightSize[%zu]",
                 workSize, weightSize);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelPath);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelId);

    gert::Om2ModelLoadArg loadArgs;
    ACL_REQUIRES_OK(ConstructOm2ModelLoadArg(workPtr, workSize, weightPtr, weightSize, loadArgs));

    const auto ret = Om2ModelLoadFromFileWithMem(modelPath, modelId, loadArgs);
    if (ret != ACL_SUCCESS) {
        ACL_LOG_ERROR("[OM2] Load OM2 model from file with mem failed, path [%s], errorCode [%u]", modelPath, ret);
    } else {
        ACL_LOG_INFO("[OM2] Load model from file[%s] with memory success, modelId[%u]", modelPath, *modelId);
    }
    return ret;
}

aclError aclmdlLoadFromMemImplOm2(const void* model, size_t modelSize, uint32_t* modelId) {
    ACL_PROFILING_REG(acl::AclProfType::AclmdlLoadFromMem);
    ACL_LOG_INFO("[OM2] start to execute aclmdlLoadFromMem, modelSize[%zu]", modelSize);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(model);
    ACL_REQUIRES_POSITIVE_WITH_INPUT_REPORT(modelSize);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelId);

    gert::Om2ModelLoadArg loadArgs;
    ACL_REQUIRES_OK(ConstructOm2ModelLoadArg(nullptr, 0U, nullptr, 0U, loadArgs));

    return Om2ModelLoadFromMemWithMem(model, modelSize, modelId, loadArgs, nullptr);
}

aclError aclmdlBundleGetModelNumImplOm2(uint32_t bundleId, size_t* modelNum) {
    (void)bundleId;
    (void)modelNum;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlBundleGetModelIdImplOm2(uint32_t bundleId, size_t index, uint32_t* modelId) {
    (void)bundleId;
    (void)index;
    (void)modelId;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclmdlBundleQueryInfo* aclmdlBundleCreateQueryInfoImplOm2() {
    return new(std::nothrow) aclmdlBundleQueryInfo();
}

aclError aclmdlBundleDestroyQueryInfoImplOm2(aclmdlBundleQueryInfo* queryInfo) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(queryInfo);
    ACL_DELETE_AND_SET_NULL(queryInfo);
    return ACL_SUCCESS;
}

aclError aclmdlBundleQueryInfoFromFileImplOm2(const char* fileName, aclmdlBundleQueryInfo* queryInfo) {
    (void)fileName;
    (void)queryInfo;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlBundleQueryInfoFromMemImplOm2(const void* model, size_t modelSize, aclmdlBundleQueryInfo* queryInfo) {
    (void)model;
    (void)modelSize;
    (void)queryInfo;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlBundleGetQueryModelNumImplOm2(const aclmdlBundleQueryInfo* queryInfo, size_t* modelNum) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(queryInfo);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelNum);
    *modelNum = queryInfo->subModelInfos.size();
    return ACL_SUCCESS;
}

aclError aclmdlBundleGetVarWeightSizeImplOm2(const aclmdlBundleQueryInfo* queryInfo, size_t* variableWeightSize) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(queryInfo);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(variableWeightSize);
    *variableWeightSize = queryInfo->varSize;
    return ACL_SUCCESS;
}

aclError aclmdlBundleGetSizeImplOm2(const aclmdlBundleQueryInfo* queryInfo, size_t index, size_t* workSize,
                                    size_t* constWeightSize) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(queryInfo);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(workSize);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(constWeightSize);
    if (index >= queryInfo->subModelInfos.size()) {
        ACL_LOG_ERROR("index %zu should be less than %zu", index, queryInfo->subModelInfos.size());
        return ACL_ERROR_INVALID_PARAM;
    }
    *workSize = queryInfo->subModelInfos[index].workSize;
    *constWeightSize = queryInfo->subModelInfos[index].weightSize;
    return ACL_SUCCESS;
}

aclError aclmdlBundleInitFromFileImplOm2(const char* modelPath, void* varWeightPtr, size_t varWeightSize,
                                         uint32_t* bundleId) {
    (void)modelPath;
    (void)varWeightPtr;
    (void)varWeightSize;
    (void)bundleId;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlBundleInitFromMemImplOm2(const void* model, size_t modelSize, void* varWeightPtr,
                                        size_t varWeightSize, uint32_t* bundleId) {
    (void)model;
    (void)modelSize;
    (void)varWeightPtr;
    (void)varWeightSize;
    (void)bundleId;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlBundleLoadModelImplOm2(uint32_t bundleId, size_t index, uint32_t* modelId) {
    (void)bundleId;
    (void)index;
    (void)modelId;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlBundleLoadModelWithMemImplOm2(uint32_t bundleId, size_t index, void* workPtr, size_t workSize,
                                             void* weightPtr, size_t weightSize, uint32_t* modelId) {
    (void)bundleId;
    (void)index;
    (void)workPtr;
    (void)workSize;
    (void)weightPtr;
    (void)weightSize;
    (void)modelId;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlBundleLoadModelWithConfigImplOm2(uint32_t bundleId, size_t index, aclmdlConfigHandle* handle,
                                                uint32_t* modelId) {
    (void)bundleId;
    (void)index;
    (void)handle;
    (void)modelId;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlBundleUnloadModelImplOm2(uint32_t bundleId, uint32_t modelId) {
    (void)bundleId;
    (void)modelId;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlLoadFromMemWithMemImplOm2(const void* model, size_t modelSize,
                                         uint32_t* modelId, void* workPtr, size_t workSize,
                                         void* weightPtr, size_t weightSize) {
    ACL_PROFILING_REG(acl::AclProfType::AclmdlLoadFromMemWithMem);
    ACL_LOG_INFO("[OM2] start...");
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(model);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelId);

    gert::Om2ModelLoadArg loadArgs;
    ACL_REQUIRES_OK(ConstructOm2ModelLoadArg(workPtr, workSize, weightPtr, weightSize, loadArgs));

    const aclError ret = Om2ModelLoadFromMemWithMem(model, modelSize, modelId, loadArgs, nullptr);
    if (ret != ACL_SUCCESS) {
        ACL_LOG_ERROR("[OM2] Load OM2 model from memory with mem failed, errorCode [%u]", ret);
    } else {
        ACL_LOG_INFO("[OM2] Successfully execute aclmdlLoadFromMemWithMem, modelId[%u]", *modelId);
    }
    return ret;
}

aclError aclmdlLoadFromFileWithQImplOm2(const char* modelPath, uint32_t* modelId, const uint32_t* inputQ,
                                        size_t inputQNum, const uint32_t* outputQ, size_t outputQNum) {
    (void)modelPath;
    (void)modelId;
    (void)inputQ;
    (void)inputQNum;
    (void)outputQ;
    (void)outputQNum;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlLoadFromMemWithQImplOm2(const void* model, size_t modelSize, uint32_t* modelId,
                                       const uint32_t* inputQ, size_t inputQNum, const uint32_t* outputQ,
                                       size_t outputQNum) {
    (void)model;
    (void)modelSize;
    (void)modelId;
    (void)inputQ;
    (void)inputQNum;
    (void)outputQ;
    (void)outputQNum;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlExecuteImplOm2(uint32_t modelId, const aclmdlDataset* input, aclmdlDataset* output) {
    ACL_PROFILING_REG(acl::AclProfType::AclmdlExecute);
    ACL_LOG_INFO("[OM2] start...");

    const aclError ret = Om2ModelExecuteCommon(modelId, input, output, false, nullptr);
    if (ret == ACL_SUCCESS) {
        ACL_LOG_INFO("[OM2] aclmdlExecute success...");
    }
    return ret;
}

aclError aclmdlExecuteV2ImplOm2(uint32_t modelId, const aclmdlDataset* input, aclmdlDataset* output, aclrtStream stream,
                                const aclmdlExecConfigHandle* handle) {
    ACL_LOG_INFO("[OM2] start...");

    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(handle);
    if (handle->streamSyncTimeout >= DEFAULT_SYNC_TIMEOUT) {
        ACL_LOG_INFO("stream synchronize timeout = %dms", handle->streamSyncTimeout);
        gert::GetOm2ThreadLocalContext().SetStreamSyncTimeout(handle->streamSyncTimeout);
    }
    if (handle->eventSyncTimeout >= DEFAULT_SYNC_TIMEOUT) {
        ACL_LOG_INFO("event synchronize timeout = %dms", handle->eventSyncTimeout);
        gert::GetOm2ThreadLocalContext().SetEventSyncTimeout(handle->eventSyncTimeout);
    }

    return Om2ModelExecuteCommon(modelId, input, output, false, stream);
}

aclError aclmdlExecuteAsyncImplOm2(uint32_t modelId, const aclmdlDataset* input,
                                   aclmdlDataset* output, aclrtStream stream) {
    ACL_PROFILING_REG(acl::AclProfType::AclmdlExecuteAsync);
    ACL_LOG_INFO("[OM2] start...");

    return Om2ModelExecuteCommon(modelId, input, output, true, stream);
}

aclError aclmdlUnloadImplOm2(uint32_t modelId) {
    ACL_PROFILING_REG(acl::AclProfType::AclmdlUnload);
    ACL_LOG_INFO("[OM2] start...");

    auto& resourceManagerV2 = acl::AclResourceManagerOm2::GetInstance();
    if (resourceManagerV2.IsBundleInnerId(modelId)) {
        ACL_LOG_ERROR("[OM2] bundle inner modelId %u, please use aclmdlBundleUnload api instead", modelId);
        return ACL_ERROR_INVALID_PARAM;
    }

    return resourceManagerV2.DeleteOm2Executor(modelId);
}

aclError aclmdlBundleUnloadImplOm2(uint32_t bundleId) {
    (void)bundleId;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlQuerySizeImplOm2(const char* fileName, size_t* workSize, size_t* weightSize) {
    ACL_PROFILING_REG(acl::AclProfType::AclmdlQuerySize);
    ACL_LOG_INFO("[OM2] start to execute aclmdlQuerySize");
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(fileName);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(workSize);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(weightSize);

    const std::string path(fileName);
    size_t work;
    size_t weight;
    const ge::Status ret = gert::GetOm2MemAndWeightSize(path, work, weight);
    if (ret != ge::SUCCESS) {
        ACL_LOG_CALL_ERROR("[Get][MemAndWeightSize]query size failed, ge result[%u]", ret);
        return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
    }

    *workSize = work;
    *weightSize = weight;
    ACL_LOG_INFO("[OM2] success to get size from file[%s], work size[%zu] bytes, weight size[%zu] bytes",
                 fileName, *workSize, *weightSize);

    return ACL_SUCCESS;
}

aclError aclmdlQuerySizeFromMemImplOm2(const void* model, size_t modelSize, size_t* workSize, size_t* weightSize) {
    ACL_PROFILING_REG(acl::AclProfType::AclmdlQuerySizeFromMem);
    ACL_LOG_INFO("[OM2] start to execute aclmdlQuerySizeFromMem, modelSize[%zu]", modelSize);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(model);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(workSize);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(weightSize);

    size_t work;
    size_t weight;
    const ge::Status ret = gert::GetOm2MemAndWeightSize(model, modelSize, work, weight);
    if (ret != ge::SUCCESS) {
        ACL_LOG_CALL_ERROR("[Get][MemAndWeightSize]query size from mem failed, ge result[%u]", ret);
        return ACL_GET_ERRCODE_GE(static_cast<int32_t>(ret));
    }

    *workSize = work;
    *weightSize = weight;
    ACL_LOG_INFO("[OM2] success to get size from mem, work size[%zu] bytes, weight size[%zu] bytes",
                 *workSize, *weightSize);

    return ACL_SUCCESS;
}

aclError aclmdlSetDynamicBatchSizeImplOm2(uint32_t modelId, aclmdlDataset* dataset, size_t index, uint64_t batchSize) {
    (void)modelId;
    (void)dataset;
    (void)index;
    (void)batchSize;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlSetDynamicHWSizeImplOm2(uint32_t modelId, aclmdlDataset* dataset, size_t index,
                                       uint64_t height, uint64_t width) {
    (void)modelId;
    (void)dataset;
    (void)index;
    (void)height;
    (void)width;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlSetInputDynamicDimsImplOm2(uint32_t modelId, aclmdlDataset* dataset, size_t index,
                                          const aclmdlIODims* dims) {
    (void)modelId;
    (void)dataset;
    (void)index;
    (void)dims;
    return ACL_ERROR_API_NOT_SUPPORT;
}

aclError aclmdlGetInputDimsImplOm2(const aclmdlDesc* modelDesc, size_t index, aclmdlIODims* dims) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dims);

    const aclError ret = acl::GetDims(modelDesc, TensorType::INPUT_TENSOR_TYPE, DimsType::DIMS_TYPE_V1, index, dims);
    if (ret != ACL_SUCCESS) {
        ACL_LOG_INNER_ERROR("[Get][Dims]get input dims failed, result[%d], index[%zu], modelId[%u]", ret, index,
                            modelDesc->modelId);
    }

    return ret;
}

aclError aclmdlGetOutputDimsImplOm2(const aclmdlDesc* modelDesc, size_t index, aclmdlIODims* dims) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dims);

    const aclError ret = acl::GetDims(modelDesc, TensorType::OUTPUT_TENSOR_TYPE, DimsType::DIMS_TYPE_V1, index, dims);
    if (ret != ACL_SUCCESS) {
        ACL_LOG_INNER_ERROR("[Get][Dims]get output dims failed, result[%d], index[%zu], modelId[%u]",
                            ret, index, modelDesc->modelId);
    }

    return ret;
}

aclError aclmdlGetInputDimsV2ImplOm2(const aclmdlDesc* modelDesc, size_t index, aclmdlIODims* dims) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dims);

    const aclError ret = acl::GetDims(modelDesc, TensorType::INPUT_TENSOR_TYPE, DimsType::DIMS_TYPE_V2, index, dims);
    if (ret != ACL_SUCCESS) {
        ACL_LOG_INNER_ERROR("[Get][Dims]get input dims(v2) failed, result[%d], index[%zu], modelId[%u]",
                            ret, index, modelDesc->modelId);
    }

    return ret;
}

aclError aclmdlGetInputDimsRangeImplOm2(const aclmdlDesc* modelDesc, size_t index, aclmdlIODimsRange* dimsRange) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dimsRange);

    const aclError ret = acl::GetDimsRange(modelDesc, TensorType::INPUT_TENSOR_TYPE, index, dimsRange);
    if (ret != ACL_SUCCESS) {
        ACL_LOG_INNER_ERROR("[Get][DimsRange]get input dims range failed, result[%d], index[%zu], modelId[%u]",
                            ret, index, modelDesc->modelId);
    }

    return ret;
}

aclError aclmdlGetCurOutputDimsImplOm2(const aclmdlDesc* modelDesc, size_t index, aclmdlIODims* dims) {
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dims);

    // OM2 currently only supports static models, directly return static dims
    ACL_LOG_DEBUG("[OM2] OM2 model detected, using static dims, modelId[%u]", modelDesc->modelId);
    aclError aclRet = GetDimsFromModelDesc(modelDesc, TensorType::OUTPUT_TENSOR_TYPE, DimsType::DIMS_TYPE_V1, index,
                                           dims);
    ACL_REQUIRES_OK_WITH_INNER_MESSAGE(aclRet,
                                       "[Get][Dims]get output dims failed for OM2 model, result[%d], index[%zu], modelId[%u]",
                                       aclRet, index, modelDesc->modelId);
    return ACL_SUCCESS;
}

const char* aclmdlGetOpAttrImplOm2(aclmdlDesc* modelDesc, const char* opName, const char* attr) {
    ACL_LOG_INFO("start to execute aclmdlGetOpAttr");
    ACL_REQUIRES_NOT_NULL_RET_NULL(modelDesc);
    ACL_REQUIRES_NOT_NULL_RET_NULL(opName);
    ACL_REQUIRES_NOT_NULL_RET_NULL(attr);

    const std::string opNameStr(opName);
    const std::string attrStr(attr);
    if (attrStr != ACL_ATTR_NAME_DATA_DUMP_ORIGIN_OP_NAMES) {
        ACL_LOG_INNER_ERROR("failed to execute aclmdlGetOpAttr, attr[%s] is invalid, only support "
            "ACL_ATTR_NAME_DATA_DUMP_ORIGIN_OP_NAMES", attrStr.c_str());
        return nullptr;
    }

    // Lookup in opAttrValueMap only
    std::map<std::string, std::map<std::string, std::string>>::const_iterator itOpName =
        modelDesc->opAttrValueMap.find(opName);
    if (itOpName != modelDesc->opAttrValueMap.cend()) {
        std::map<std::string, std::string>::const_iterator itAttr = itOpName->second.find(attr);
        if (itAttr != itOpName->second.cend()) {
            ACL_LOG_INFO("opName is [%s], the value of attr [%s] is %s", opName, attr, itAttr->second.c_str());
            return itAttr->second.c_str();
        }
    }
    return nullptr;
}

const char* aclmdlGetInputNameByIndexImplOm2(const aclmdlDesc* modelDesc, size_t index) {
    ACL_LOG_INFO("start to execute aclmdlGetInputNameByIndex");
    if (modelDesc == nullptr) {
        ACL_LOG_ERROR("[Check][ModelDesc]modelDesc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
                                                  std::vector<const char*>({"param"}),
                                                  std::vector<const char*>({"modelDesc"}));
        return "";
    }

    return acl::aclmdlGetNameByIndex(modelDesc->inputDesc, index);
}

const char* aclmdlGetOutputNameByIndexImplOm2(const aclmdlDesc* modelDesc, size_t index) {
    ACL_LOG_INFO("start to execute aclmdlGetOutputNameByIndex");
    if (modelDesc == nullptr) {
        ACL_LOG_ERROR("[Check][ModelDesc]modelDesc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
                                                  std::vector<const char*>({"param"}),
                                                  std::vector<const char*>({"modelDesc"}));
        return "";
    }

    return acl::aclmdlGetNameByIndex(modelDesc->outputDesc, index);
}

aclFormat aclmdlGetInputFormatImplOm2(const aclmdlDesc* modelDesc, size_t index) {
    ACL_LOG_INFO("start to execute aclmdlGetInputFormat");
    if (modelDesc == nullptr) {
        ACL_LOG_ERROR("[Check][ModelDesc]modelDesc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
                                                  std::vector<const char*>({"param"}),
                                                  std::vector<const char*>({"modelDesc"}));
        return ACL_FORMAT_UNDEFINED;
    }

    return acl::aclmdlGetFormat(modelDesc->inputDesc, index);
}

aclFormat aclmdlGetOutputFormatImplOm2(const aclmdlDesc* modelDesc, size_t index) {
    ACL_LOG_INFO("start to execute aclmdlGetOutputFormat");
    if (modelDesc == nullptr) {
        ACL_LOG_ERROR("[Check][ModelDesc]modelDesc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
                                                  std::vector<const char*>({"params"}),
                                                  std::vector<const char*>({"modelDesc"}));
        return ACL_FORMAT_UNDEFINED;
    }

    return acl::aclmdlGetFormat(modelDesc->outputDesc, index);
}

aclDataType aclmdlGetInputDataTypeImplOm2(const aclmdlDesc* modelDesc, size_t index) {
    ACL_LOG_INFO("start to execute aclmdlGetInputDataType");
    if (modelDesc == nullptr) {
        ACL_LOG_ERROR("[Check][ModelDesc]modelDesc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
                                                  std::vector<const char*>({"param"}),
                                                  std::vector<const char*>({"modelDesc"}));
        return ACL_DT_UNDEFINED;
    }

    return acl::aclmdlGetDataType(modelDesc->inputDesc, index);
}

aclDataType aclmdlGetOutputDataTypeImplOm2(const aclmdlDesc* modelDesc, size_t index) {
    ACL_LOG_INFO("start to execute aclmdlGetOutputDataType");
    if (modelDesc == nullptr) {
        ACL_LOG_ERROR("[Check][ModelDesc]modelDesc is null");
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_NULL_POINTER_MSG,
                                                  std::vector<const char*>({"param"}),
                                                  std::vector<const char*>({"modelDesc"}));
        return ACL_DT_UNDEFINED;
    }

    return acl::aclmdlGetDataType(modelDesc->outputDesc, index);
}

aclError aclmdlGetInputIndexByNameImplOm2(const aclmdlDesc* modelDesc, const char* name, size_t* index) {
    ACL_LOG_INFO("start to execute aclmdlGetInputIndexByName");
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(name);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(index);

    ACL_LOG_INFO("successfully execute aclmdlGetInputIndexByName");
    return acl::aclmdlGetIndexByName(modelDesc->inputDesc, name, index);
}

aclError aclmdlGetOutputIndexByNameImplOm2(const aclmdlDesc* modelDesc, const char* name, size_t* index) {
    ACL_LOG_INFO("start to execute aclmdlGetOutputIndexByName");
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(name);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(index);

    ACL_LOG_INFO("successfully execute aclmdlGetOutputIndexByName");
    return acl::aclmdlGetIndexByName(modelDesc->outputDesc, name, index);
}

aclError aclmdlGetDynamicBatchImplOm2(const aclmdlDesc* modelDesc, aclmdlBatch* batch) {
    ACL_LOG_INFO("start to execute aclmdlGetDynamicBatch");
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(batch);

    const size_t batchCnt = modelDesc->dynamicBatch.size();
    if (batchCnt > static_cast<size_t>(ACL_MAX_BATCH_NUM)) {
        ACL_LOG_ERROR("[Check][batchCnt]aclmdlGetBatch failed, batch count[%zu] is larger than max batch num[%d]",
                      batchCnt, ACL_MAX_BATCH_NUM);
        const std::string errMsg =
            acl::AclErrorLogManager::FormatStr("batch count[%zu] is larger than max batch num[%d]",
                                               batchCnt, ACL_MAX_BATCH_NUM);
        acl::AclErrorLogManager::ReportInputError(acl::INVALID_PARAM_MSG,
                                                  std::vector<const char*>({"param", "value", "reason"}),
                                                  std::vector<const char*>({
                                                      "batch count", std::to_string(batchCnt).c_str(), errMsg.c_str()
                                                  }));
        return ACL_ERROR_STORAGE_OVER_LIMIT;
    }

    batch->batchCount = batchCnt;
    if (batchCnt == 0U) {
        ACL_LOG_INFO("batch count is 0");
        return ACL_SUCCESS;
    }

    for (size_t i = 0U; i < batchCnt; ++i) {
        batch->batch[i] = modelDesc->dynamicBatch[i];
    }

    ACL_LOG_INFO("successfully execute aclmdlGetDynamicBatch");
    return ACL_SUCCESS;
}

aclError aclmdlGetDynamicHWImplOm2(const aclmdlDesc* modelDesc, size_t index, aclmdlHW* hw) {
    (void)index;
    ACL_LOG_INFO("start to execute aclmdlGetDynamicHW");
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(hw);

    const size_t hwCnt = modelDesc->dynamicHW.size();
    if (hwCnt > static_cast<size_t>(ACL_MAX_HW_NUM)) {
        ACL_LOG_INNER_ERROR("[Check][hwCnt]aclmdlGetHW failed, hw count[%zu] is larger than max[%d]",
                            hwCnt, ACL_MAX_HW_NUM);
        return ACL_ERROR_STORAGE_OVER_LIMIT;
    }

    hw->hwCount = hwCnt;
    if (hwCnt == 0U) {
        ACL_LOG_INFO("hw count is 0");
        return ACL_SUCCESS;
    }

    for (size_t i = 0U; i < hwCnt; ++i) {
        for (size_t j = 0U; j < 2U; ++j) {
            // dynamic hw,size is 2
            hw->hw[i][j] = modelDesc->dynamicHW[i][j];
        }
    }
    ACL_LOG_INFO("successfully execute aclmdlGetDynamicHW");
    return ACL_SUCCESS;
}

aclError aclmdlGetInputDynamicGearCountImplOm2(const aclmdlDesc* modelDesc, size_t index, size_t* gearCount) {
    ACL_LOG_INFO("start to execute aclmdlGetInputDynamicGearCount");
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(gearCount);

    if (index != static_cast<size_t>(-1)) {
        ACL_LOG_INNER_ERROR("[Check][index]aclmdlGetInputDynamicGearCount failed, index must be -1 while input is %zu.",
                            index);
        return ACL_ERROR_INVALID_PARAM;
    }

    const size_t dimCnt = modelDesc->dynamicDims.size();
    if (dimCnt > static_cast<size_t>(ACL_MAX_DIM_CNT)) {
        ACL_LOG_INNER_ERROR("[Check][dimCnt]aclmdlGetInputDynamicGearCount failed, dimCnt[%zu] is "
                            "larger than max[%d]", dimCnt, ACL_MAX_DIM_CNT);
        return ACL_ERROR_STORAGE_OVER_LIMIT;
    }

    if (dimCnt == 0U) {
        *gearCount = 0U;
        ACL_LOG_INFO("Gear count is 0");
        return ACL_SUCCESS;
    }
    *gearCount = dimCnt;
    ACL_LOG_INFO("successfully execute aclmdlGetInputDynamicGearCount");
    return ACL_SUCCESS;
}

aclError aclmdlGetInputDynamicDimsImplOm2(const aclmdlDesc* modelDesc, size_t index, aclmdlIODims* dims,
                                          size_t gearCount) {
    ACL_LOG_INFO("start to execute aclmdlGetInputDynamicDims");
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dims);
    if (index != static_cast<std::size_t>(-1)) {
        ACL_LOG_INNER_ERROR("[Check][index]aclmdlGetInputDynamicDims failed, index must be -1 but it is %zu", index);
        return ACL_ERROR_INVALID_PARAM;
    }
    if (gearCount < modelDesc->dynamicDims.size()) {
        ACL_LOG_INNER_ERROR("[Check][gearCount]Gear count[%zu] can not less than model's dynamic gear count[%zu]",
                            gearCount, modelDesc->dynamicDims.size());
        return ACL_ERROR_INVALID_PARAM;
    }
    std::vector<int64_t> allRawDims;
    for (auto& dataName : modelDesc->dataNameOrder) {
        for (auto& inputDesc : modelDesc->inputDesc) {
            if (inputDesc.name == dataName) {
                (void)allRawDims.insert(allRawDims.cend(), inputDesc.dims.cbegin(), inputDesc.dims.cend());
            }
        }
    }
    if (allRawDims.size() > ACL_MAX_DIM_CNT) {
        ACL_LOG_ERROR("current dynamic dims size %zu can not be larger than %d", allRawDims.size(), ACL_MAX_DIM_CNT);
        return ACL_ERROR_FAILURE;
    }

    for (size_t i = 0U; i < modelDesc->dynamicDims.size(); ++i) {
        size_t begIndex = 0U;
        for (size_t j = 0U; j < allRawDims.size(); ++j) {
            if (allRawDims[j] < 0) {
                if (begIndex >= modelDesc->dynamicDims[i].size()) {
                    ACL_LOG_INNER_ERROR("[Check][begIndex]User input data index[%zu] shape size overflow", index);
                    return ACL_ERROR_INVALID_PARAM;
                }
                dims[i].dims[j] = static_cast<int64_t>(modelDesc->dynamicDims[i][begIndex]);
                begIndex++;
            } else {
                dims[i].dims[j] = allRawDims[j];
            }
            dims[i].dimCount = allRawDims.size();
        }
    }
    ACL_LOG_INFO("successfully execute aclmdlGetInputDynamicDims");
    return ACL_SUCCESS;
}

aclError aclmdlCreateAndGetOpDescImplOm2(uint32_t deviceId, uint32_t streamId, uint32_t taskId, char* opName,
                                         size_t opNameLen, aclTensorDesc** inputDesc, size_t* numInputs,
                                         aclTensorDesc** outputDesc, size_t* numOutputs) {
    ACL_LOG_INFO("[OM2] start to execute aclmdlCreateAndGetOpDesc, deviceId[%u], streamId[%u], taskId[%u]",
                 deviceId, streamId, taskId);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(opName);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(inputDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(outputDesc);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(numInputs);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(numOutputs);

    ge::OpDescInfo opDescInfo;
    ACL_REQUIRES_OK(acl::AclResourceManagerOm2::GetInstance().GetOpDescInfo(deviceId, streamId, taskId, opDescInfo));

    if (opNameLen <= opDescInfo.op_name.length()) {
        ACL_LOG_INNER_ERROR("[Check][opNameLen]input length = %zu must be larger than op name real length = %zu",
                            opNameLen, opDescInfo.op_name.length());
        return ACL_ERROR_INVALID_PARAM;
    }
    const auto copyRet = strncpy_s(opName, opNameLen, opDescInfo.op_name.c_str(), opDescInfo.op_name.length());
    if (copyRet != EOK) {
        ACL_LOG_INNER_ERROR("[Copy][OpName]copy op name failed, copy errorCode = %d, input opNameLen = %zu, "
                            "real opNameLen = %zu", copyRet, opNameLen, opDescInfo.op_name.length());
        return ACL_ERROR_FAILURE;
    }

    ACL_REQUIRES_OK(CreateAclTensorDescFromOpDescInfo(opDescInfo, inputDesc, numInputs, outputDesc, numOutputs));
    ACL_LOG_INFO("[OM2] successfully execute aclmdlCreateAndGetOpDesc, deviceId[%u], streamId[%u], "
                 "taskId[%u], numInputs[%zu], numOutputs[%zu]", deviceId, streamId, taskId, *numInputs, *numOutputs);
    return ACL_SUCCESS;
}

aclError aclmdlLoadWithConfigImplOm2(const aclmdlConfigHandle* handle, uint32_t* modelId) {
    ACL_PROFILING_REG(acl::AclProfType::AclmdlLoadWithConfig);
    ACL_LOG_INFO("start to execute aclmdlLoadWithConfig");
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(handle);
    ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(modelId);
    if (!acl::CheckMdlConfigHandle(handle)) {
        ACL_LOG_ERROR("[Check][ConfigHandle]model config is invalid because some params may not be set or invalid");
        return ACL_ERROR_INVALID_PARAM;
    }
    auto& file_constant_mems = handle->fileConstantMem;
    for (const auto& file_constant_mem : file_constant_mems) {
        ACL_LOG_INFO("file constant name[%s], device memory address[%p], device memory size[%zu]",
                     file_constant_mem.file_name.c_str(), file_constant_mem.device_mem, file_constant_mem.mem_size);
    }
    switch (static_cast<int32_t>(handle->mdlLoadType)) {
    case ACL_MDL_LOAD_FROM_FILE:
        return acl::LoadFromFile(handle, file_constant_mems, modelId);
    case ACL_MDL_LOAD_FROM_FILE_WITH_MEM:
        return acl::LoadFromFileWithMem(handle, file_constant_mems, modelId);
    case ACL_MDL_LOAD_FROM_MEM:
        return acl::LoadFromMem(handle, file_constant_mems, modelId);
    case ACL_MDL_LOAD_FROM_MEM_WITH_MEM:
        return acl::LoadFromMemWithMem(handle, file_constant_mems, modelId);
    case ACL_MDL_LOAD_FROM_FILE_WITH_Q:
        return ACL_ERROR_API_NOT_SUPPORT;
    case ACL_MDL_LOAD_FROM_MEM_WITH_Q:
        return ACL_ERROR_API_NOT_SUPPORT;
    default:
        ACL_LOG_INNER_ERROR("[Load][Model]model load type[%zu] is invalid, it should be in [%d, %d]",
                            handle->mdlLoadType, ACL_MDL_LOAD_FROM_FILE, ACL_MDL_LOAD_FROM_MEM_WITH_Q);
        return ACL_ERROR_INVALID_PARAM;
    }
}

const char* aclmdlGetTensorRealNameImplOm2(const aclmdlDesc* modelDesc, const char* name) {
    ACL_LOG_INFO("start to execute aclmdlGetTensorName");
    ACL_REQUIRES_NOT_NULL_RET_NULL_INPUT_REPORT(modelDesc);
    ACL_REQUIRES_NOT_NULL_RET_NULL_INPUT_REPORT(name);
    const char_t* realTensorName = acl::GetRealTensorName(modelDesc, name);
    if (realTensorName != nullptr) {
        ACL_LOG_INFO("successfully execute aclmdlGetTensorName, realTensorName = %s", realTensorName);
        return realTensorName;
    }
    realTensorName = acl::TransTensorNameToReal(modelDesc, name);
    if (realTensorName != nullptr) {
        ACL_LOG_INFO("successfully execute aclmdlGetTensorName, realTensorName = %s", realTensorName);
        return realTensorName;
    }
    ACL_LOG_INNER_ERROR("[Get][TensorName]execute aclmdlGetTensorName failed, name[%s] is invalid.", name);
    return nullptr;
}

aclError aclRecoverAllHcclTasksImplOm2(int32_t deviceId) {
    (void)deviceId;
    return ACL_SUCCESS;
}
