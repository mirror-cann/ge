/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACL_MODEL_SRC_MODEL_ACL_MODEL_IMPL_OM2_H_
#define ACL_MODEL_SRC_MODEL_ACL_MODEL_IMPL_OM2_H_

#include <cstddef>
#include <cstdint>

#include "acl_base_rt.h"
#include "acl_rt.h"
#include "acl_mdl.h"

#ifdef __cplusplus
extern "C" {
#endif

ACL_FUNC_VISIBILITY aclmdlDesc *aclmdlCreateDescImplOm2();

ACL_FUNC_VISIBILITY aclError aclmdlDestroyDescImplOm2(aclmdlDesc *modelDesc);

ACL_FUNC_VISIBILITY aclError aclmdlGetDescImplOm2(aclmdlDesc *modelDesc, uint32_t modelId);

ACL_FUNC_VISIBILITY aclError aclmdlGetDescFromFileImplOm2(aclmdlDesc *modelDesc, const char *modelPath);

ACL_FUNC_VISIBILITY aclError aclmdlGetDescFromMemImplOm2(aclmdlDesc *modelDesc, const void *model, size_t modelSize);

ACL_FUNC_VISIBILITY size_t aclmdlGetNumInputsImplOm2(aclmdlDesc *modelDesc);

ACL_FUNC_VISIBILITY size_t aclmdlGetNumOutputsImplOm2(aclmdlDesc *modelDesc);

ACL_FUNC_VISIBILITY size_t aclmdlGetInputSizeByIndexImplOm2(aclmdlDesc *modelDesc, size_t index);

ACL_FUNC_VISIBILITY size_t aclmdlGetOutputSizeByIndexImplOm2(aclmdlDesc *modelDesc, size_t index);

ACL_FUNC_VISIBILITY aclmdlExecConfigHandle *aclmdlCreateExecConfigHandleImplOm2();

ACL_FUNC_VISIBILITY aclError aclmdlDestroyExecConfigHandleImplOm2(const aclmdlExecConfigHandle *handle);

ACL_FUNC_VISIBILITY aclmdlDataset *aclmdlCreateDatasetImplOm2();

ACL_FUNC_VISIBILITY aclError aclmdlDestroyDatasetImplOm2(const aclmdlDataset *dataset);

ACL_FUNC_VISIBILITY aclError aclmdlAddDatasetBufferImplOm2(aclmdlDataset *dataset, aclDataBuffer *dataBuffer);

ACL_FUNC_VISIBILITY aclError aclmdlSetDatasetTensorDescImplOm2(aclmdlDataset *dataset, aclTensorDesc *tensorDesc,
                                                               size_t index);

ACL_FUNC_VISIBILITY aclTensorDesc *aclmdlGetDatasetTensorDescImplOm2(const aclmdlDataset *dataset, size_t index);

ACL_FUNC_VISIBILITY size_t aclmdlGetDatasetNumBuffersImplOm2(const aclmdlDataset *dataset);

ACL_FUNC_VISIBILITY aclDataBuffer *aclmdlGetDatasetBufferImplOm2(const aclmdlDataset *dataset, size_t index);

ACL_FUNC_VISIBILITY aclError aclmdlLoadFromFileImplOm2(const char *modelPath, uint32_t *modelId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleLoadFromFileImplOm2(const char *modelPath, uint32_t *bundleId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleLoadFromMemImplOm2(const void *model, size_t modelSize, uint32_t *bundleId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleUnloadImplOm2(uint32_t bundleId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleGetModelNumImplOm2(uint32_t bundleId, size_t *modelNum);

ACL_FUNC_VISIBILITY aclError aclmdlBundleGetModelIdImplOm2(uint32_t bundleId, size_t index, uint32_t *modelId);

ACL_FUNC_VISIBILITY aclmdlBundleQueryInfo *aclmdlBundleCreateQueryInfoImplOm2();

ACL_FUNC_VISIBILITY aclError aclmdlBundleDestroyQueryInfoImplOm2(aclmdlBundleQueryInfo *queryInfo);

ACL_FUNC_VISIBILITY aclError aclmdlBundleQueryInfoFromFileImplOm2(const char *fileName,
                                                                  aclmdlBundleQueryInfo *queryInfo);

ACL_FUNC_VISIBILITY aclError aclmdlBundleQueryInfoFromMemImplOm2(const void *model, size_t modelSize,
                                                                 aclmdlBundleQueryInfo *queryInfo);

ACL_FUNC_VISIBILITY aclError aclmdlBundleGetQueryModelNumImplOm2(const aclmdlBundleQueryInfo *queryInfo,
                                                                 size_t *modelNum);

ACL_FUNC_VISIBILITY aclError aclmdlBundleGetVarWeightSizeImplOm2(const aclmdlBundleQueryInfo *queryInfo,
                                                                 size_t *variableWeightSize);

ACL_FUNC_VISIBILITY aclError aclmdlBundleGetSizeImplOm2(const aclmdlBundleQueryInfo *queryInfo, size_t index,
                                                        size_t *workSize, size_t *constWeightSize);

ACL_FUNC_VISIBILITY aclError aclmdlBundleInitFromFileImplOm2(const char *modelPath, void *varWeightPtr,
                                                             size_t varWeightSize, uint32_t *bundleId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleInitFromMemImplOm2(const void *model, size_t modelSize, void *varWeightPtr,
                                                            size_t varWeightSize, uint32_t *bundleId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleLoadModelImplOm2(uint32_t bundleId, size_t index, uint32_t *modelId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleLoadModelWithMemImplOm2(uint32_t bundleId, size_t index, void *workPtr,
                                                                 size_t workSize, void *weightPtr, size_t weightSize,
                                                                 uint32_t *modelId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleLoadModelWithConfigImplOm2(uint32_t bundleId, size_t index,
                                                                    aclmdlConfigHandle *handle, uint32_t *modelId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleUnloadModelImplOm2(uint32_t bundleId, uint32_t modelId);

ACL_FUNC_VISIBILITY aclError aclmdlLoadFromMemImplOm2(const void *model, size_t modelSize, uint32_t *modelId);

ACL_FUNC_VISIBILITY aclError aclmdlLoadFromFileWithMemImplOm2(const char *modelPath, uint32_t *modelId, void *workPtr,
                                                              size_t workSize, void *weightPtr, size_t weightSize);

ACL_FUNC_VISIBILITY aclError aclmdlLoadFromMemWithMemImplOm2(const void *model, size_t modelSize, uint32_t *modelId,
                                                             void *workPtr, size_t workSize, void *weightPtr,
                                                             size_t weightSize);

ACL_FUNC_VISIBILITY aclError aclmdlLoadFromFileWithQImplOm2(const char *modelPath, uint32_t *modelId,
                                                            const uint32_t *inputQ, size_t inputQNum,
                                                            const uint32_t *outputQ, size_t outputQNum);

ACL_FUNC_VISIBILITY aclError aclmdlLoadFromMemWithQImplOm2(const void *model, size_t modelSize, uint32_t *modelId,
                                                           const uint32_t *inputQ, size_t inputQNum,
                                                           const uint32_t *outputQ, size_t outputQNum);

ACL_FUNC_VISIBILITY aclError aclmdlExecuteImplOm2(uint32_t modelId, const aclmdlDataset *input, aclmdlDataset *output);

ACL_FUNC_VISIBILITY aclError aclmdlExecuteV2ImplOm2(uint32_t modelId, const aclmdlDataset *input, aclmdlDataset *output,
                                                    aclrtStream stream, const aclmdlExecConfigHandle *handle);

ACL_FUNC_VISIBILITY aclError aclmdlExecuteAsyncImplOm2(uint32_t modelId, const aclmdlDataset *input,
                                                       aclmdlDataset *output, aclrtStream stream);

ACL_FUNC_VISIBILITY aclError aclmdlUnloadImplOm2(uint32_t modelId);

ACL_FUNC_VISIBILITY aclError aclmdlQuerySizeImplOm2(const char *fileName, size_t *workSize, size_t *weightSize);

ACL_FUNC_VISIBILITY aclError aclmdlQuerySizeFromMemImplOm2(const void *model, size_t modelSize, size_t *workSize,
                                                           size_t *weightSize);

ACL_FUNC_VISIBILITY aclError aclmdlSetDynamicBatchSizeImplOm2(uint32_t modelId, aclmdlDataset *dataset, size_t index,
                                                              uint64_t batchSize);

ACL_FUNC_VISIBILITY aclError aclmdlSetDynamicHWSizeImplOm2(uint32_t modelId, aclmdlDataset *dataset, size_t index,
                                                           uint64_t height, uint64_t width);

ACL_FUNC_VISIBILITY aclError aclmdlSetInputDynamicDimsImplOm2(uint32_t modelId, aclmdlDataset *dataset, size_t index,
                                                              const aclmdlIODims *dims);

ACL_FUNC_VISIBILITY aclError aclmdlGetInputDimsImplOm2(const aclmdlDesc *modelDesc, size_t index, aclmdlIODims *dims);

ACL_FUNC_VISIBILITY aclError aclmdlGetInputDimsV2ImplOm2(const aclmdlDesc *modelDesc, size_t index, aclmdlIODims *dims);

ACL_FUNC_VISIBILITY aclError aclmdlGetInputDimsRangeImplOm2(const aclmdlDesc *modelDesc, size_t index,
                                                            aclmdlIODimsRange *dimsRange);

ACL_FUNC_VISIBILITY aclError aclmdlGetOutputDimsImplOm2(const aclmdlDesc *modelDesc, size_t index, aclmdlIODims *dims);

ACL_FUNC_VISIBILITY aclError aclmdlGetCurOutputDimsImplOm2(const aclmdlDesc *modelDesc, size_t index,
                                                           aclmdlIODims *dims);

ACL_FUNC_VISIBILITY const char *aclmdlGetOpAttrImplOm2(aclmdlDesc *modelDesc, const char *opName, const char *attr);

ACL_FUNC_VISIBILITY const char *aclmdlGetInputNameByIndexImplOm2(const aclmdlDesc *modelDesc, size_t index);

ACL_FUNC_VISIBILITY const char *aclmdlGetOutputNameByIndexImplOm2(const aclmdlDesc *modelDesc, size_t index);

ACL_FUNC_VISIBILITY aclFormat aclmdlGetInputFormatImplOm2(const aclmdlDesc *modelDesc, size_t index);

ACL_FUNC_VISIBILITY aclFormat aclmdlGetOutputFormatImplOm2(const aclmdlDesc *modelDesc, size_t index);

ACL_FUNC_VISIBILITY aclDataType aclmdlGetInputDataTypeImplOm2(const aclmdlDesc *modelDesc, size_t index);

ACL_FUNC_VISIBILITY aclDataType aclmdlGetOutputDataTypeImplOm2(const aclmdlDesc *modelDesc, size_t index);

ACL_FUNC_VISIBILITY aclError aclmdlGetInputIndexByNameImplOm2(const aclmdlDesc *modelDesc, const char *name,
                                                              size_t *index);

ACL_FUNC_VISIBILITY aclError aclmdlGetOutputIndexByNameImplOm2(const aclmdlDesc *modelDesc, const char *name,
                                                               size_t *index);

ACL_FUNC_VISIBILITY aclError aclmdlGetDynamicBatchImplOm2(const aclmdlDesc *modelDesc, aclmdlBatch *batch);

ACL_FUNC_VISIBILITY aclError aclmdlGetDynamicHWImplOm2(const aclmdlDesc *modelDesc, size_t index, aclmdlHW *hw);

ACL_FUNC_VISIBILITY aclError aclmdlGetInputDynamicGearCountImplOm2(const aclmdlDesc *modelDesc, size_t index,
                                                                   size_t *gearCount);

ACL_FUNC_VISIBILITY aclError aclmdlGetInputDynamicDimsImplOm2(const aclmdlDesc *modelDesc, size_t index,
                                                              aclmdlIODims *dims, size_t gearCount);

ACL_FUNC_VISIBILITY aclmdlAIPP *aclmdlCreateAIPPImplOm2(uint64_t batchSize);

ACL_FUNC_VISIBILITY aclError aclmdlDestroyAIPPImplOm2(const aclmdlAIPP *aippParmsSet);

ACL_FUNC_VISIBILITY aclError aclmdlGetAippDataSizeImplOm2(uint64_t batchSize, size_t *size);

ACL_FUNC_VISIBILITY aclError aclmdlSetAIPPInputFormatImplOm2(aclmdlAIPP *aippParmsSet, aclAippInputFormat inputFormat);

ACL_FUNC_VISIBILITY aclError aclmdlSetAIPPCscParamsImplOm2(
    aclmdlAIPP *aippParmsSet, int8_t cscSwitch, int16_t cscMatrixR0C0, int16_t cscMatrixR0C1, int16_t cscMatrixR0C2,
    int16_t cscMatrixR1C0, int16_t cscMatrixR1C1, int16_t cscMatrixR1C2, int16_t cscMatrixR2C0, int16_t cscMatrixR2C1,
    int16_t cscMatrixR2C2, uint8_t cscOutputBiasR0, uint8_t cscOutputBiasR1, uint8_t cscOutputBiasR2,
    uint8_t cscInputBiasR0, uint8_t cscInputBiasR1, uint8_t cscInputBiasR2);

ACL_FUNC_VISIBILITY aclError aclmdlSetAIPPRbuvSwapSwitchImplOm2(aclmdlAIPP *aippParmsSet, int8_t rbuvSwapSwitch);

ACL_FUNC_VISIBILITY aclError aclmdlSetAIPPAxSwapSwitchImplOm2(aclmdlAIPP *aippParmsSet, int8_t axSwapSwitch);

ACL_FUNC_VISIBILITY aclError aclmdlSetAIPPSrcImageSizeImplOm2(aclmdlAIPP *aippParmsSet, int32_t srcImageSizeW,
                                                              int32_t srcImageSizeH);

ACL_FUNC_VISIBILITY aclError aclmdlSetAIPPScfParamsImplOm2(aclmdlAIPP *aippParmsSet, int8_t scfSwitch,
                                                           int32_t scfInputSizeW, int32_t scfInputSizeH,
                                                           int32_t scfOutputSizeW, int32_t scfOutputSizeH,
                                                           uint64_t batchIndex);

ACL_FUNC_VISIBILITY aclError aclmdlSetAIPPCropParamsImplOm2(aclmdlAIPP *aippParmsSet, int8_t cropSwitch,
                                                            int32_t cropStartPosW, int32_t cropStartPosH,
                                                            int32_t cropSizeW, int32_t cropSizeH, uint64_t batchIndex);

ACL_FUNC_VISIBILITY aclError aclmdlSetAIPPPaddingParamsImplOm2(aclmdlAIPP *aippParmsSet, int8_t paddingSwitch,
                                                               int32_t paddingSizeTop, int32_t paddingSizeBottom,
                                                               int32_t paddingSizeLeft, int32_t paddingSizeRight,
                                                               uint64_t batchIndex);

ACL_FUNC_VISIBILITY aclError aclmdlSetAIPPDtcPixelMeanImplOm2(aclmdlAIPP *aippParmsSet, int16_t dtcPixelMeanChn0,
                                                              int16_t dtcPixelMeanChn1, int16_t dtcPixelMeanChn2,
                                                              int16_t dtcPixelMeanChn3, uint64_t batchIndex);

ACL_FUNC_VISIBILITY aclError aclmdlSetAIPPDtcPixelMinImplOm2(aclmdlAIPP *aippParmsSet, float dtcPixelMinChn0,
                                                             float dtcPixelMinChn1, float dtcPixelMinChn2,
                                                             float dtcPixelMinChn3, uint64_t batchIndex);

ACL_FUNC_VISIBILITY aclError aclmdlSetAIPPPixelVarReciImplOm2(aclmdlAIPP *aippParmsSet, float dtcPixelVarReciChn0,
                                                              float dtcPixelVarReciChn1, float dtcPixelVarReciChn2,
                                                              float dtcPixelVarReciChn3, uint64_t batchIndex);

ACL_FUNC_VISIBILITY aclError aclmdlSetInputAIPPImplOm2(uint32_t modelId, aclmdlDataset *dataset, size_t index,
                                                       const aclmdlAIPP *aippParmsSet);

ACL_FUNC_VISIBILITY aclError aclmdlSetAIPPByInputIndexImplOm2(uint32_t modelId, aclmdlDataset *dataset, size_t index,
                                                              const aclmdlAIPP *aippParmsSet);

ACL_FUNC_VISIBILITY aclError aclmdlGetAippTypeImplOm2(uint32_t modelId, size_t index, aclmdlInputAippType *type,
                                                      size_t *dynamicAttachedDataIndex);

ACL_FUNC_VISIBILITY aclError aclmdlGetFirstAippInfoImplOm2(uint32_t modelId, size_t index, aclAippInfo *aippInfo);

ACL_FUNC_VISIBILITY aclError aclmdlCreateAndGetOpDescImplOm2(uint32_t deviceId, uint32_t streamId, uint32_t taskId,
                                                             char *opName, size_t opNameLen, aclTensorDesc **inputDesc,
                                                             size_t *numInputs, aclTensorDesc **outputDesc,
                                                             size_t *numOutputs);

ACL_FUNC_VISIBILITY aclError aclmdlLoadWithConfigImplOm2(const aclmdlConfigHandle *handle, uint32_t *modelId);

ACL_FUNC_VISIBILITY aclError aclmdlSetExternalWeightAddressImplOm2(aclmdlConfigHandle *handle,
                                                                   const char *weightFileName, void *devPtr,
                                                                   size_t size);

ACL_FUNC_VISIBILITY aclmdlConfigHandle *aclmdlCreateConfigHandleImplOm2();

ACL_FUNC_VISIBILITY aclError aclmdlDestroyConfigHandleImplOm2(aclmdlConfigHandle *handle);

ACL_FUNC_VISIBILITY aclError aclmdlSetConfigOptImplOm2(aclmdlConfigHandle *handle, aclmdlConfigAttr attr,
                                                       const void *attrValue, size_t valueSize);

ACL_FUNC_VISIBILITY aclError aclmdlSetExecConfigOptImplOm2(aclmdlExecConfigHandle *handle, aclmdlExecConfigAttr attr,
                                                           const void *attrValue, size_t valueSize);

ACL_FUNC_VISIBILITY const char *aclmdlGetTensorRealNameImplOm2(const aclmdlDesc *modelDesc, const char *name);

ACL_FUNC_VISIBILITY aclError aclRecoverAllHcclTasksImplOm2(int32_t deviceId);

ACL_FUNC_VISIBILITY aclTensorDesc *aclCreateTensorDescImplOm2(aclDataType dataType, int numDims, const int64_t *dims,
                                                              aclFormat format);

ACL_FUNC_VISIBILITY void aclDestroyTensorDescImplOm2(const aclTensorDesc *desc);

ACL_FUNC_VISIBILITY aclError aclSetTensorShapeRangeImplOm2(aclTensorDesc *desc, size_t dimsCount,
                                                           int64_t dimsRange[][ACL_TENSOR_SHAPE_RANGE_NUM]);

ACL_FUNC_VISIBILITY aclError aclSetTensorValueRangeImplOm2(aclTensorDesc *desc, size_t valueCount,
                                                           int64_t valueRange[][ACL_TENSOR_VALUE_RANGE_NUM]);

ACL_FUNC_VISIBILITY aclDataType aclGetTensorDescTypeImplOm2(const aclTensorDesc *desc);

ACL_FUNC_VISIBILITY aclFormat aclGetTensorDescFormatImplOm2(const aclTensorDesc *desc);

ACL_FUNC_VISIBILITY size_t aclGetTensorDescSizeImplOm2(const aclTensorDesc *desc);

ACL_FUNC_VISIBILITY size_t aclGetTensorDescElementCountImplOm2(const aclTensorDesc *desc);

ACL_FUNC_VISIBILITY size_t aclGetTensorDescNumDimsImplOm2(const aclTensorDesc *desc);

ACL_FUNC_VISIBILITY int64_t aclGetTensorDescDimImplOm2(const aclTensorDesc *desc, size_t index);

ACL_FUNC_VISIBILITY aclError aclGetTensorDescDimV2ImplOm2(const aclTensorDesc *desc, size_t index, int64_t *dimSize);

ACL_FUNC_VISIBILITY aclError aclGetTensorDescDimRangeImplOm2(const aclTensorDesc *desc, size_t index,
                                                             size_t dimRangeNum, int64_t *dimRange);

ACL_FUNC_VISIBILITY void aclSetTensorDescNameImplOm2(aclTensorDesc *desc, const char *name);

ACL_FUNC_VISIBILITY const char *aclGetTensorDescNameImplOm2(aclTensorDesc *desc);

ACL_FUNC_VISIBILITY aclError aclTransTensorDescFormatImplOm2(const aclTensorDesc *srcDesc, aclFormat dstFormat,
                                                             aclTensorDesc **dstDesc);

ACL_FUNC_VISIBILITY aclError aclSetTensorStorageFormatImplOm2(aclTensorDesc *desc, aclFormat format);

ACL_FUNC_VISIBILITY aclError aclSetTensorStorageShapeImplOm2(aclTensorDesc *desc, int numDims, const int64_t *dims);

ACL_FUNC_VISIBILITY aclError aclSetTensorFormatImplOm2(aclTensorDesc *desc, aclFormat format);

ACL_FUNC_VISIBILITY aclError aclSetTensorShapeImplOm2(aclTensorDesc *desc, int numDims, const int64_t *dims);

ACL_FUNC_VISIBILITY aclError aclSetTensorOriginFormatImplOm2(aclTensorDesc *desc, aclFormat format);

ACL_FUNC_VISIBILITY aclError aclSetTensorOriginShapeImplOm2(aclTensorDesc *desc, int numDims, const int64_t *dims);

ACL_FUNC_VISIBILITY aclTensorDesc *aclGetTensorDescByIndexImplOm2(aclTensorDesc *desc, size_t index);

ACL_FUNC_VISIBILITY void *aclGetTensorDescAddressImplOm2(const aclTensorDesc *desc);

ACL_FUNC_VISIBILITY aclError aclSetTensorDynamicInputImplOm2(aclTensorDesc *desc, const char *dynamicInputName);

ACL_FUNC_VISIBILITY aclError aclSetTensorConstImplOm2(aclTensorDesc *desc, void *dataBuffer, size_t length);

ACL_FUNC_VISIBILITY aclError aclSetTensorPlaceMentImplOm2(aclTensorDesc *desc, aclMemType memType);

ACL_FUNC_VISIBILITY aclError aclmdlSetAttributeImplOm2(uint32_t modelId, aclmdlAttr attr, aclmdlAttrValue_t *attrValue);

ACL_FUNC_VISIBILITY aclError aclmdlGetAttributeImplOm2(uint32_t modelId, aclmdlAttr attr, aclmdlAttrValue_t *attrValue);

#ifdef __cplusplus
}
#endif

#endif  // ACL_MODEL_SRC_MODEL_ACL_MODEL_IMPL_OM2_H_
