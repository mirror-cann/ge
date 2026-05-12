/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "acl_model_impl.h"
#include "acl_model_impl_om2.h"
#include "model_common.h"
#include "model_desc_internal.h"
#include "acl_resource_manager_om2.h"

#ifdef __cplusplus
extern "C" {
#endif

aclmdlDesc *aclmdlCreateDesc()
{
    return aclmdlCreateDescImplOm2();
}

aclError aclmdlDestroyDesc(aclmdlDesc *modelDesc)
{
    return aclmdlDestroyDescImplOm2(modelDesc);
}

aclError aclmdlGetDesc(aclmdlDesc *modelDesc, uint32_t modelId)
{
    return AclIsOm2ModelById(modelId) ?
        aclmdlGetDescImplOm2(modelDesc, modelId) :
        aclmdlGetDescImpl(modelDesc, modelId);
}

aclError aclmdlGetDescFromFile(aclmdlDesc *modelDesc, const char *modelPath)
{
    return AclIsOm2ModelByPath(modelPath) ?
        aclmdlGetDescFromFileImplOm2(modelDesc, modelPath) :
        aclmdlGetDescFromFileImpl(modelDesc, modelPath);
}

aclError aclmdlGetDescFromMem(aclmdlDesc *modelDesc, const void *model, size_t modelSize)
{
    return AclIsOm2ModelByData(model, modelSize) ?
        aclmdlGetDescFromMemImplOm2(modelDesc, model, modelSize) :
        aclmdlGetDescFromMemImpl(modelDesc, model, modelSize);
}

size_t aclmdlGetNumInputs(aclmdlDesc *modelDesc)
{
    return aclmdlGetNumInputsImplOm2(modelDesc);
}

size_t aclmdlGetNumOutputs(aclmdlDesc *modelDesc)
{
    return aclmdlGetNumOutputsImplOm2(modelDesc);
}

size_t aclmdlGetInputSizeByIndex(aclmdlDesc *modelDesc, size_t index)
{
    return aclmdlGetInputSizeByIndexImplOm2(modelDesc, index);
}

size_t aclmdlGetOutputSizeByIndex(aclmdlDesc *modelDesc, size_t index)
{
    return aclmdlGetOutputSizeByIndexImplOm2(modelDesc, index);
}

aclmdlExecConfigHandle *aclmdlCreateExecConfigHandle()
{
    return aclmdlCreateExecConfigHandleImplOm2();
}

aclError aclmdlDestroyExecConfigHandle(const aclmdlExecConfigHandle *handle)
{
    return aclmdlDestroyExecConfigHandleImplOm2(handle);
}

aclmdlDataset *aclmdlCreateDataset()
{
    return aclmdlCreateDatasetImplOm2();
}

aclError aclmdlDestroyDataset(const aclmdlDataset *dataset)
{
    return aclmdlDestroyDatasetImplOm2(dataset);
}

aclError aclmdlAddDatasetBuffer(aclmdlDataset *dataset, aclDataBuffer *dataBuffer)
{
    return aclmdlAddDatasetBufferImplOm2(dataset, dataBuffer);
}

aclError aclmdlSetDatasetTensorDesc(aclmdlDataset *dataset, aclTensorDesc *tensorDesc, size_t index)
{
    return aclmdlSetDatasetTensorDescImplOm2(dataset, tensorDesc, index);
}

aclTensorDesc *aclmdlGetDatasetTensorDesc(const aclmdlDataset *dataset, size_t index)
{
    return aclmdlGetDatasetTensorDescImplOm2(dataset, index);
}

size_t aclmdlGetDatasetNumBuffers(const aclmdlDataset *dataset)
{
    return aclmdlGetDatasetNumBuffersImplOm2(dataset);
}

aclDataBuffer *aclmdlGetDatasetBuffer(const aclmdlDataset *dataset, size_t index)
{
    return aclmdlGetDatasetBufferImplOm2(dataset, index);
}

aclError aclmdlLoadFromFile(const char *modelPath, uint32_t *modelId)
{
    return AclIsOm2ModelByPath(modelPath) ?
        aclmdlLoadFromFileImplOm2(modelPath, modelId) :
        aclmdlLoadFromFileImpl(modelPath, modelId);
}

aclError aclmdlBundleLoadFromFile(const char *modelPath, uint32_t *bundleId)
{
    return AclIsOm2ModelByPath(modelPath) ?
        aclmdlBundleLoadFromFileImplOm2(modelPath, bundleId) :
        aclmdlBundleLoadFromFileImpl(modelPath, bundleId);
}

aclError aclmdlBundleLoadFromMem(const void *model,  size_t modelSize, uint32_t *bundleId)
{
    return AclIsOm2ModelByData(model, modelSize) ?
        aclmdlBundleLoadFromMemImplOm2(model, modelSize, bundleId) :
        aclmdlBundleLoadFromMemImpl(model, modelSize, bundleId);
}

aclError aclmdlBundleUnload(uint32_t bundleId)
{
    return AclIsOm2BundleById(bundleId) ?
        aclmdlBundleUnloadImplOm2(bundleId) :
        aclmdlBundleUnloadImpl(bundleId);
}

aclError aclmdlBundleGetModelNum(uint32_t bundleId, size_t *modelNum)
{
    return AclIsOm2BundleById(bundleId) ?
        aclmdlBundleGetModelNumImplOm2(bundleId, modelNum) :
        aclmdlBundleGetModelNumImpl(bundleId, modelNum);
}

aclError aclmdlBundleGetModelId(uint32_t bundleId, size_t index, uint32_t *modelId)
{
    return AclIsOm2BundleById(bundleId) ?
        aclmdlBundleGetModelIdImplOm2(bundleId, index, modelId) :
        aclmdlBundleGetModelIdImpl(bundleId, index, modelId);
}

aclmdlBundleQueryInfo *aclmdlBundleCreateQueryInfo()
{
    return aclmdlBundleCreateQueryInfoImplOm2();
}

aclError aclmdlBundleDestroyQueryInfo(aclmdlBundleQueryInfo *queryInfo)
{
  return aclmdlBundleDestroyQueryInfoImplOm2(queryInfo);
}

aclError aclmdlBundleQueryInfoFromFile(const char* fileName, aclmdlBundleQueryInfo *queryInfo)
{
    return AclIsOm2ModelByPath(fileName) ?
        aclmdlBundleQueryInfoFromFileImplOm2(fileName, queryInfo) :
        aclmdlBundleQueryInfoFromFileImpl(fileName, queryInfo);
}

aclError aclmdlBundleQueryInfoFromMem(const void *model, size_t modelSize, aclmdlBundleQueryInfo *queryInfo)
{
    return AclIsOm2ModelByData(model, modelSize) ?
        aclmdlBundleQueryInfoFromMemImplOm2(model, modelSize, queryInfo) :
        aclmdlBundleQueryInfoFromMemImpl(model, modelSize, queryInfo);
}

aclError aclmdlBundleGetQueryModelNum(const aclmdlBundleQueryInfo *queryInfo, size_t *modelNum)
{
  return aclmdlBundleGetQueryModelNumImplOm2(queryInfo, modelNum);
}

aclError aclmdlBundleGetVarWeightSize(const aclmdlBundleQueryInfo *queryInfo, size_t *variableWeightSize)
{
  return aclmdlBundleGetVarWeightSizeImplOm2(queryInfo, variableWeightSize);
}

aclError aclmdlBundleGetSize(const aclmdlBundleQueryInfo *queryInfo, size_t index,
                             size_t *workSize, size_t *constWeightSize)
{
  return aclmdlBundleGetSizeImplOm2(queryInfo, index, workSize, constWeightSize);
}

aclError aclmdlBundleInitFromFile(const char* modelPath, void *varWeightPtr,
                                  size_t varWeightSize, uint32_t *bundleId)
{
    return AclIsOm2ModelByPath(modelPath) ?
        aclmdlBundleInitFromFileImplOm2(modelPath, varWeightPtr, varWeightSize, bundleId) :
        aclmdlBundleInitFromFileImpl(modelPath, varWeightPtr, varWeightSize, bundleId);
}

aclError aclmdlBundleInitFromMem(const void* model, size_t modelSize, void *varWeightPtr,
                                 size_t varWeightSize, uint32_t *bundleId)
{
    return AclIsOm2ModelByData(model, modelSize) ?
        aclmdlBundleInitFromMemImplOm2(model, modelSize, varWeightPtr, varWeightSize, bundleId) :
        aclmdlBundleInitFromMemImpl(model, modelSize, varWeightPtr, varWeightSize, bundleId);
}

aclError aclmdlBundleLoadModel(uint32_t bundleId, size_t index, uint32_t *modelId)
{
    return AclIsOm2BundleById(bundleId) ?
        aclmdlBundleLoadModelImplOm2(bundleId, index, modelId) :
        aclmdlBundleLoadModelImpl(bundleId, index, modelId);
}

aclError aclmdlBundleLoadModelWithMem(uint32_t bundleId, size_t index, void *workPtr,
                                      size_t workSize, void *weightPtr,
                                      size_t weightSize, uint32_t *modelId)
{
    return AclIsOm2BundleById(bundleId) ?
        aclmdlBundleLoadModelWithMemImplOm2(bundleId, index, workPtr, workSize, weightPtr, weightSize, modelId) :
        aclmdlBundleLoadModelWithMemImpl(bundleId, index, workPtr, workSize, weightPtr, weightSize, modelId);
}

aclError aclmdlBundleLoadModelWithConfig(uint32_t bundleId, size_t index,
                                         aclmdlConfigHandle *handle, uint32_t *modelId)
{
    return AclIsOm2BundleById(bundleId) ?
        aclmdlBundleLoadModelWithConfigImplOm2(bundleId, index, handle, modelId) :
        aclmdlBundleLoadModelWithConfigImpl(bundleId, index, handle, modelId);
}

aclError aclmdlBundleUnloadModel(uint32_t bundleId, uint32_t modelId)
{
    return AclIsOm2BundleById(bundleId) ?
        aclmdlBundleUnloadModelImplOm2(bundleId, modelId) :
        aclmdlBundleUnloadModelImpl(bundleId, modelId);
}

aclError aclmdlLoadFromMem(const void *model,  size_t modelSize, uint32_t *modelId)
{
    return AclIsOm2ModelByData(model, modelSize) ?
        aclmdlLoadFromMemImplOm2(model, modelSize, modelId) :
        aclmdlLoadFromMemImpl(model, modelSize, modelId);
}

aclError aclmdlLoadFromFileWithMem(const char *modelPath,
                                   uint32_t *modelId, void *workPtr, size_t workSize,
                                   void *weightPtr, size_t weightSize)
{
    return AclIsOm2ModelByPath(modelPath) ?
        aclmdlLoadFromFileWithMemImplOm2(modelPath, modelId, workPtr, workSize, weightPtr, weightSize) :
        aclmdlLoadFromFileWithMemImpl(modelPath, modelId, workPtr, workSize, weightPtr, weightSize);
}

aclError aclmdlLoadFromMemWithMem(const void *model, size_t modelSize,
                                  uint32_t *modelId, void *workPtr, size_t workSize,
                                  void *weightPtr, size_t weightSize)
{
    return AclIsOm2ModelByData(model, modelSize) ?
        aclmdlLoadFromMemWithMemImplOm2(model, modelSize, modelId, workPtr, workSize, weightPtr, weightSize) :
        aclmdlLoadFromMemWithMemImpl(model, modelSize, modelId, workPtr, workSize, weightPtr, weightSize);
}

aclError aclmdlLoadFromFileWithQ(const char *modelPath, uint32_t *modelId, const uint32_t *inputQ,
                                 size_t inputQNum, const uint32_t *outputQ, size_t outputQNum)
{
    return AclIsOm2ModelByPath(modelPath) ?
        aclmdlLoadFromFileWithQImplOm2(modelPath, modelId, inputQ, inputQNum, outputQ, outputQNum) :
        aclmdlLoadFromFileWithQImpl(modelPath, modelId, inputQ, inputQNum, outputQ, outputQNum);
}

aclError aclmdlLoadFromMemWithQ(const void *model, size_t modelSize, uint32_t *modelId,
                                const uint32_t *inputQ, size_t inputQNum,
                                const uint32_t *outputQ, size_t outputQNum)
{
    return AclIsOm2ModelByData(model, modelSize) ?
        aclmdlLoadFromMemWithQImplOm2(model, modelSize, modelId, inputQ, inputQNum, outputQ, outputQNum) :
        aclmdlLoadFromMemWithQImpl(model, modelSize, modelId, inputQ, inputQNum, outputQ, outputQNum);
}

aclError aclmdlExecute(uint32_t modelId, const aclmdlDataset *input, aclmdlDataset *output)
{
    return AclIsOm2ModelById(modelId) ?
        aclmdlExecuteImplOm2(modelId, input, output) :
        aclmdlExecuteImpl(modelId, input, output);
}

aclError aclmdlExecuteV2(uint32_t modelId, const aclmdlDataset *input, aclmdlDataset *output,
                         aclrtStream stream, const aclmdlExecConfigHandle *handle)
{
    return AclIsOm2ModelById(modelId) ?
        aclmdlExecuteV2ImplOm2(modelId, input, output, stream, handle) :
        aclmdlExecuteV2Impl(modelId, input, output, stream, handle);
}

aclError aclmdlExecuteAsync(uint32_t modelId, const aclmdlDataset *input,
                            aclmdlDataset *output, aclrtStream stream)
{
    return AclIsOm2ModelById(modelId) ?
        aclmdlExecuteAsyncImplOm2(modelId, input, output, stream) :
        aclmdlExecuteAsyncImpl(modelId, input, output, stream);
}

aclError aclmdlUnload(uint32_t modelId)
{
    return AclIsOm2ModelById(modelId) ?
        aclmdlUnloadImplOm2(modelId) :
        aclmdlUnloadImpl(modelId);
}

aclError aclmdlQuerySize(const char *fileName, size_t *workSize, size_t *weightSize)
{
    return AclIsOm2ModelByPath(fileName) ?
        aclmdlQuerySizeImplOm2(fileName, workSize, weightSize) :
        aclmdlQuerySizeImpl(fileName, workSize, weightSize);
}

aclError aclmdlQuerySizeFromMem(const void *model, size_t modelSize, size_t *workSize,
                                size_t *weightSize)
{
    return AclIsOm2ModelByData(model, modelSize) ?
        aclmdlQuerySizeFromMemImplOm2(model, modelSize, workSize, weightSize) :
        aclmdlQuerySizeFromMemImpl(model, modelSize, workSize, weightSize);
}

aclError aclmdlSetDynamicBatchSize(uint32_t modelId, aclmdlDataset *dataset, size_t index,
                                   uint64_t batchSize)
{
    return AclIsOm2ModelById(modelId) ?
        aclmdlSetDynamicBatchSizeImplOm2(modelId, dataset, index, batchSize) :
        aclmdlSetDynamicBatchSizeImpl(modelId, dataset, index, batchSize);
}

aclError aclmdlSetDynamicHWSize(uint32_t modelId, aclmdlDataset *dataset, size_t index,
                                uint64_t height, uint64_t width)
{
    return AclIsOm2ModelById(modelId) ?
        aclmdlSetDynamicHWSizeImplOm2(modelId, dataset, index, height, width) :
        aclmdlSetDynamicHWSizeImpl(modelId, dataset, index, height, width);
}

aclError aclmdlSetInputDynamicDims(uint32_t modelId, aclmdlDataset *dataset, size_t index,
                                   const aclmdlIODims *dims)
{
    return AclIsOm2ModelById(modelId) ?
        aclmdlSetInputDynamicDimsImplOm2(modelId, dataset, index, dims) :
        aclmdlSetInputDynamicDimsImpl(modelId, dataset, index, dims);
}

aclError aclmdlGetInputDims(const aclmdlDesc *modelDesc, size_t index, aclmdlIODims *dims)
{
    return aclmdlGetInputDimsImplOm2(modelDesc, index, dims);
}

aclError aclmdlGetInputDimsV2(const aclmdlDesc *modelDesc, size_t index, aclmdlIODims *dims)
{
    return aclmdlGetInputDimsV2ImplOm2(modelDesc, index, dims);
}

aclError aclmdlGetInputDimsRange(const aclmdlDesc *modelDesc, size_t index,
                                 aclmdlIODimsRange *dimsRange)
{
    return aclmdlGetInputDimsRangeImplOm2(modelDesc, index, dimsRange);
}

aclError aclmdlGetOutputDims(const aclmdlDesc *modelDesc, size_t index, aclmdlIODims *dims)
{
    return aclmdlGetOutputDimsImplOm2(modelDesc, index, dims);
}

aclError aclmdlGetCurOutputDims(const aclmdlDesc *modelDesc, size_t index, aclmdlIODims *dims)
{
    return aclmdlGetCurOutputDimsImpl(modelDesc, index, dims);
}

const char *aclmdlGetOpAttr(aclmdlDesc *modelDesc, const char *opName, const char *attr)
{
    if (modelDesc == nullptr) {
        return nullptr;
    }
    return AclIsOm2ModelById(modelDesc->modelId) ?
        aclmdlGetOpAttrImplOm2(modelDesc, opName, attr) :
        aclmdlGetOpAttrImpl(modelDesc, opName, attr);
}

const char *aclmdlGetInputNameByIndex(const aclmdlDesc *modelDesc, size_t index)
{
    return aclmdlGetInputNameByIndexImplOm2(modelDesc, index);
}

const char *aclmdlGetOutputNameByIndex(const aclmdlDesc *modelDesc, size_t index)
{
    return aclmdlGetOutputNameByIndexImplOm2(modelDesc, index);
}

aclFormat aclmdlGetInputFormat(const aclmdlDesc *modelDesc, size_t index)
{
    return aclmdlGetInputFormatImplOm2(modelDesc, index);
}

aclFormat aclmdlGetOutputFormat(const aclmdlDesc *modelDesc, size_t index)
{
    return aclmdlGetOutputFormatImplOm2(modelDesc, index);
}

aclDataType aclmdlGetInputDataType(const aclmdlDesc *modelDesc, size_t index)
{
    return aclmdlGetInputDataTypeImplOm2(modelDesc, index);
}

aclDataType aclmdlGetOutputDataType(const aclmdlDesc *modelDesc, size_t index)
{
    return aclmdlGetOutputDataTypeImplOm2(modelDesc, index);
}

aclError aclmdlGetInputIndexByName(const aclmdlDesc *modelDesc, const char *name, size_t *index)
{
    return aclmdlGetInputIndexByNameImplOm2(modelDesc, name, index);
}

aclError aclmdlGetOutputIndexByName(const aclmdlDesc *modelDesc, const char *name, size_t *index)
{
    return aclmdlGetOutputIndexByNameImplOm2(modelDesc, name, index);
}

aclError aclmdlGetDynamicBatch(const aclmdlDesc *modelDesc, aclmdlBatch *batch)
{
    return aclmdlGetDynamicBatchImplOm2(modelDesc, batch);
}

aclError aclmdlGetDynamicHW(const aclmdlDesc *modelDesc, size_t index, aclmdlHW *hw)
{
    return aclmdlGetDynamicHWImplOm2(modelDesc, index, hw);
}

aclError aclmdlGetInputDynamicGearCount(const aclmdlDesc *modelDesc, size_t index,
                                        size_t *gearCount)
{
    return aclmdlGetInputDynamicGearCountImplOm2(modelDesc, index, gearCount);
}

aclError aclmdlGetInputDynamicDims(const aclmdlDesc *modelDesc, size_t index, aclmdlIODims *dims,
                                   size_t gearCount)
{
    return aclmdlGetInputDynamicDimsImplOm2(modelDesc, index, dims, gearCount);
}

aclmdlAIPP *aclmdlCreateAIPP(uint64_t batchSize)
{
    return aclmdlCreateAIPPImplOm2(batchSize);
}

aclError aclmdlDestroyAIPP(const aclmdlAIPP *aippParmsSet)
{
    return aclmdlDestroyAIPPImplOm2(aippParmsSet);
}

aclError aclmdlGetAippDataSize(uint64_t batchSize, size_t *size)
{
    return aclmdlGetAippDataSizeImplOm2(batchSize, size);
}

aclError aclmdlSetAIPPInputFormat(aclmdlAIPP *aippParmsSet, aclAippInputFormat inputFormat)
{
    return aclmdlSetAIPPInputFormatImplOm2(aippParmsSet, inputFormat);
}

aclError aclmdlSetAIPPCscParams(aclmdlAIPP *aippParmsSet, int8_t cscSwitch,
                                int16_t cscMatrixR0C0, int16_t cscMatrixR0C1, int16_t cscMatrixR0C2,
                                int16_t cscMatrixR1C0, int16_t cscMatrixR1C1, int16_t cscMatrixR1C2,
                                int16_t cscMatrixR2C0, int16_t cscMatrixR2C1, int16_t cscMatrixR2C2,
                                uint8_t cscOutputBiasR0, uint8_t cscOutputBiasR1,
                                uint8_t cscOutputBiasR2, uint8_t cscInputBiasR0,
                                uint8_t cscInputBiasR1, uint8_t cscInputBiasR2)
{
    return aclmdlSetAIPPCscParamsImplOm2(aippParmsSet, cscSwitch, cscMatrixR0C0, cscMatrixR0C1, cscMatrixR0C2, cscMatrixR1C0, cscMatrixR1C1, cscMatrixR1C2, cscMatrixR2C0, cscMatrixR2C1, cscMatrixR2C2, cscOutputBiasR0, cscOutputBiasR1, cscOutputBiasR2, cscInputBiasR0, cscInputBiasR1, cscInputBiasR2);
}

aclError aclmdlSetAIPPRbuvSwapSwitch(aclmdlAIPP *aippParmsSet, int8_t rbuvSwapSwitch)
{
    return aclmdlSetAIPPRbuvSwapSwitchImplOm2(aippParmsSet, rbuvSwapSwitch);
}

aclError aclmdlSetAIPPAxSwapSwitch(aclmdlAIPP *aippParmsSet, int8_t axSwapSwitch)
{
    return aclmdlSetAIPPAxSwapSwitchImplOm2(aippParmsSet, axSwapSwitch);
}

aclError aclmdlSetAIPPSrcImageSize(aclmdlAIPP *aippParmsSet, int32_t srcImageSizeW,
                                   int32_t srcImageSizeH)
{
    return aclmdlSetAIPPSrcImageSizeImplOm2(aippParmsSet, srcImageSizeW, srcImageSizeH);
}

aclError aclmdlSetAIPPScfParams(aclmdlAIPP *aippParmsSet,
                                int8_t scfSwitch,
                                int32_t scfInputSizeW,
                                int32_t scfInputSizeH,
                                int32_t scfOutputSizeW,
                                int32_t scfOutputSizeH,
                                uint64_t batchIndex)
{
    return aclmdlSetAIPPScfParamsImplOm2(aippParmsSet, scfSwitch, scfInputSizeW, scfInputSizeH, scfOutputSizeW, scfOutputSizeH, batchIndex);
}

aclError aclmdlSetAIPPCropParams(aclmdlAIPP *aippParmsSet,
                                 int8_t cropSwitch,
                                 int32_t cropStartPosW,
                                 int32_t cropStartPosH,
                                 int32_t cropSizeW,
                                 int32_t cropSizeH,
                                 uint64_t batchIndex)
{
    return aclmdlSetAIPPCropParamsImplOm2(aippParmsSet, cropSwitch, cropStartPosW, cropStartPosH, cropSizeW, cropSizeH, batchIndex);
}

aclError aclmdlSetAIPPPaddingParams(aclmdlAIPP *aippParmsSet, int8_t paddingSwitch,
                                    int32_t paddingSizeTop, int32_t paddingSizeBottom,
                                    int32_t paddingSizeLeft, int32_t paddingSizeRight,
                                    uint64_t batchIndex)
{
    return aclmdlSetAIPPPaddingParamsImplOm2(aippParmsSet, paddingSwitch, paddingSizeTop, paddingSizeBottom, paddingSizeLeft, paddingSizeRight, batchIndex);
}

aclError aclmdlSetAIPPDtcPixelMean(aclmdlAIPP *aippParmsSet,
                                   int16_t dtcPixelMeanChn0,
                                   int16_t dtcPixelMeanChn1,
                                   int16_t dtcPixelMeanChn2,
                                   int16_t dtcPixelMeanChn3,
                                   uint64_t batchIndex)
{
    return aclmdlSetAIPPDtcPixelMeanImplOm2(aippParmsSet, dtcPixelMeanChn0, dtcPixelMeanChn1, dtcPixelMeanChn2, dtcPixelMeanChn3, batchIndex);
}

aclError aclmdlSetAIPPDtcPixelMin(aclmdlAIPP *aippParmsSet,
                                  float dtcPixelMinChn0,
                                  float dtcPixelMinChn1,
                                  float dtcPixelMinChn2,
                                  float dtcPixelMinChn3,
                                  uint64_t batchIndex)
{
    return aclmdlSetAIPPDtcPixelMinImplOm2(aippParmsSet, dtcPixelMinChn0, dtcPixelMinChn1, dtcPixelMinChn2, dtcPixelMinChn3, batchIndex);
}

aclError aclmdlSetAIPPPixelVarReci(aclmdlAIPP *aippParmsSet,
                                   float dtcPixelVarReciChn0,
                                   float dtcPixelVarReciChn1,
                                   float dtcPixelVarReciChn2,
                                   float dtcPixelVarReciChn3,
                                   uint64_t batchIndex)
{
    return aclmdlSetAIPPPixelVarReciImplOm2(aippParmsSet, dtcPixelVarReciChn0, dtcPixelVarReciChn1, dtcPixelVarReciChn2, dtcPixelVarReciChn3, batchIndex);
}

aclError aclmdlSetInputAIPP(uint32_t modelId,
                            aclmdlDataset *dataset,
                            size_t index,
                            const aclmdlAIPP *aippParmsSet)
{
    return AclIsOm2ModelById(modelId) ?
        aclmdlSetInputAIPPImplOm2(modelId, dataset, index, aippParmsSet) :
        aclmdlSetInputAIPPImpl(modelId, dataset, index, aippParmsSet);
}

aclError aclmdlSetAIPPByInputIndex(uint32_t modelId,
                                   aclmdlDataset *dataset,
                                   size_t index,
                                   const aclmdlAIPP *aippParmsSet)
{
    return AclIsOm2ModelById(modelId) ?
        aclmdlSetAIPPByInputIndexImplOm2(modelId, dataset, index, aippParmsSet) :
        aclmdlSetAIPPByInputIndexImpl(modelId, dataset, index, aippParmsSet);
}

aclError aclmdlGetAippType(uint32_t modelId,
                           size_t index,
                           aclmdlInputAippType *type,
                           size_t *dynamicAttachedDataIndex)
{
    return AclIsOm2ModelById(modelId) ?
        aclmdlGetAippTypeImplOm2(modelId, index, type, dynamicAttachedDataIndex) :
        aclmdlGetAippTypeImpl(modelId, index, type, dynamicAttachedDataIndex);
}

aclError aclmdlGetFirstAippInfo(uint32_t modelId, size_t index, aclAippInfo *aippInfo)
{
    return AclIsOm2ModelById(modelId) ?
        aclmdlGetFirstAippInfoImplOm2(modelId, index, aippInfo) :
        aclmdlGetFirstAippInfoImpl(modelId, index, aippInfo);
}

aclError aclmdlCreateAndGetOpDesc(uint32_t deviceId, uint32_t streamId,
    uint32_t taskId, char *opName, size_t opNameLen, aclTensorDesc **inputDesc, size_t *numInputs,
    aclTensorDesc **outputDesc, size_t *numOutputs)
{
    return aclmdlCreateAndGetOpDescImpl(deviceId, streamId, taskId, opName, opNameLen, inputDesc, numInputs, outputDesc, numOutputs);
}

aclError aclmdlLoadWithConfig(const aclmdlConfigHandle *handle, uint32_t *modelId)
{
    return AclIsOm2ModelByConfig(handle) ?
        aclmdlLoadWithConfigImplOm2(handle, modelId) :
        aclmdlLoadWithConfigImpl(handle, modelId);
}

aclError aclmdlSetExternalWeightAddress(aclmdlConfigHandle *handle, const char *weightFileName,
    void *devPtr, size_t size)
{
    return aclmdlSetExternalWeightAddressImplOm2(handle, weightFileName, devPtr, size);
}

aclmdlConfigHandle *aclmdlCreateConfigHandle()
{
    return aclmdlCreateConfigHandleImplOm2();
}

aclError aclmdlDestroyConfigHandle(aclmdlConfigHandle *handle)
{
    return aclmdlDestroyConfigHandleImplOm2(handle);
}

aclError aclmdlSetConfigOpt(aclmdlConfigHandle *handle, aclmdlConfigAttr attr,
    const void *attrValue, size_t valueSize)
{
    return aclmdlSetConfigOptImplOm2(handle, attr, attrValue, valueSize);
}

aclError aclmdlSetExecConfigOpt(aclmdlExecConfigHandle *handle, aclmdlExecConfigAttr attr,
                                const void *attrValue, size_t valueSize)
{
    return aclmdlSetExecConfigOptImplOm2(handle, attr, attrValue, valueSize);
}

const char *aclmdlGetTensorRealName(const aclmdlDesc *modelDesc, const char *name)
{
    return aclmdlGetTensorRealNameImplOm2(modelDesc, name);
}

aclError aclRecoverAllHcclTasks(int32_t deviceId)
{
    aclError ret = aclRecoverAllHcclTasksImpl(deviceId);
    if (ret != ACL_SUCCESS) {
        return ret;
    }
    return aclRecoverAllHcclTasksImplOm2(deviceId);
}

aclTensorDesc *aclCreateTensorDesc(aclDataType dataType, int numDims, const int64_t *dims, aclFormat format)
{
    return aclCreateTensorDescImplOm2(dataType, numDims, dims, format);
}

void aclDestroyTensorDesc(const aclTensorDesc *desc)
{
    aclDestroyTensorDescImplOm2(desc);
}

aclError aclSetTensorShapeRange(aclTensorDesc* desc,
                                size_t dimsCount,
                                int64_t dimsRange[][ACL_TENSOR_SHAPE_RANGE_NUM])
{
    return aclSetTensorShapeRangeImplOm2(desc, dimsCount, dimsRange);
}

aclError aclSetTensorValueRange(aclTensorDesc* desc,
                                size_t valueCount,
                                int64_t valueRange[][ACL_TENSOR_VALUE_RANGE_NUM])
{
    return aclSetTensorValueRangeImplOm2(desc, valueCount, valueRange);
}

aclDataType aclGetTensorDescType(const aclTensorDesc *desc)
{
    return aclGetTensorDescTypeImplOm2(desc);
}

aclFormat aclGetTensorDescFormat(const aclTensorDesc *desc)
{
    return aclGetTensorDescFormatImplOm2(desc);
}

size_t aclGetTensorDescSize(const aclTensorDesc *desc)
{
    return aclGetTensorDescSizeImplOm2(desc);
}

size_t aclGetTensorDescElementCount(const aclTensorDesc *desc)
{
    return aclGetTensorDescElementCountImplOm2(desc);
}

size_t aclGetTensorDescNumDims(const aclTensorDesc *desc)
{
    return aclGetTensorDescNumDimsImplOm2(desc);
}

int64_t aclGetTensorDescDim(const aclTensorDesc *desc, size_t index)
{
    return aclGetTensorDescDimImplOm2(desc, index);
}

aclError aclGetTensorDescDimV2(const aclTensorDesc *desc, size_t index, int64_t *dimSize)
{
    return aclGetTensorDescDimV2ImplOm2(desc, index, dimSize);
}

aclError aclGetTensorDescDimRange(const aclTensorDesc* desc,
                                  size_t index,
                                  size_t dimRangeNum,
                                  int64_t *dimRange)
{
    return aclGetTensorDescDimRangeImplOm2(desc, index, dimRangeNum, dimRange);
}

void aclSetTensorDescName(aclTensorDesc *desc, const char *name)
{
    aclSetTensorDescNameImplOm2(desc, name);
}

const char *aclGetTensorDescName(aclTensorDesc *desc)
{
    return aclGetTensorDescNameImplOm2(desc);
}

aclError aclTransTensorDescFormat(const aclTensorDesc *srcDesc, aclFormat dstFormat,
                                  aclTensorDesc **dstDesc)
{
    return aclTransTensorDescFormatImpl(srcDesc, dstFormat, dstDesc);
}

aclError aclSetTensorStorageFormat(aclTensorDesc *desc, aclFormat format)
{
    return aclSetTensorStorageFormatImplOm2(desc, format);
}

aclError aclSetTensorStorageShape(aclTensorDesc *desc, int numDims, const int64_t *dims)
{
    return aclSetTensorStorageShapeImplOm2(desc, numDims, dims);
}

aclError aclSetTensorFormat(aclTensorDesc *desc, aclFormat format)
{
    return aclSetTensorFormatImplOm2(desc, format);
}

aclError aclSetTensorShape(aclTensorDesc *desc, int numDims, const int64_t *dims)
{
    return aclSetTensorShapeImplOm2(desc, numDims, dims);
}

aclError aclSetTensorOriginFormat(aclTensorDesc *desc, aclFormat format)
{
    return aclSetTensorOriginFormatImplOm2(desc, format);
}

aclError aclSetTensorOriginShape(aclTensorDesc *desc, int numDims, const int64_t *dims)
{
    return aclSetTensorOriginShapeImplOm2(desc, numDims, dims);
}

aclTensorDesc *aclGetTensorDescByIndex(aclTensorDesc *desc, size_t index)
{
    return aclGetTensorDescByIndexImplOm2(desc, index);
}

aclError aclSetTensorDynamicInput(aclTensorDesc *desc, const char *dynamicInputName)
{
    return aclSetTensorDynamicInputImplOm2(desc, dynamicInputName);
}

aclError aclSetTensorConst(aclTensorDesc *desc, void *dataBuffer, size_t length)
{
    return aclSetTensorConstImplOm2(desc, dataBuffer, length);
}

aclError aclSetTensorPlaceMent(aclTensorDesc *desc, aclMemType memType)
{
    return aclSetTensorPlaceMentImplOm2(desc, memType);
}

void *aclGetTensorDescAddress(const aclTensorDesc *desc)
{
    return aclGetTensorDescAddressImplOm2(desc);
}

#ifdef __cplusplus
}
#endif
