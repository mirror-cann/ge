/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACL_MODEL_SRC_MODEL_ACL_MODEL_IMPL_H_
#define ACL_MODEL_SRC_MODEL_ACL_MODEL_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "acl_base_rt.h"
#include "acl_rt.h"
#include "acl_mdl.h"

#ifdef __cplusplus
extern "C" {
#endif

ACL_FUNC_VISIBILITY aclError aclmdlGetDescImpl(aclmdlDesc *modelDesc, uint32_t modelId);

ACL_FUNC_VISIBILITY aclError aclmdlGetDescFromFileImpl(aclmdlDesc *modelDesc, const char *modelPath);

ACL_FUNC_VISIBILITY aclError aclmdlGetDescFromMemImpl(aclmdlDesc *modelDesc, const void *model, size_t modelSize);

ACL_FUNC_VISIBILITY aclError aclmdlSetDatasetTensorDescImpl(aclmdlDataset *dataset,
                                                        aclTensorDesc *tensorDesc,
                                                        size_t index);

ACL_FUNC_VISIBILITY aclError aclmdlLoadFromFileImpl(const char *modelPath, uint32_t *modelId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleLoadFromFileImpl(const char *modelPath, uint32_t *bundleId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleLoadFromMemImpl(const void *model,  size_t modelSize, uint32_t *bundleId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleUnloadImpl(uint32_t bundleId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleGetModelNumImpl(uint32_t bundleId, size_t *modelNum);

ACL_FUNC_VISIBILITY aclError aclmdlBundleGetModelIdImpl(uint32_t bundleId, size_t index, uint32_t *modelId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleQueryInfoFromFileImpl(const char* fileName, aclmdlBundleQueryInfo *queryInfo);

ACL_FUNC_VISIBILITY aclError aclmdlBundleQueryInfoFromMemImpl(const void *model, size_t modelSize,
                                                              aclmdlBundleQueryInfo *queryInfo);

ACL_FUNC_VISIBILITY aclError aclmdlBundleInitFromFileImpl(const char* modelPath, void *varWeightPtr,
                                                          size_t varWeightSize, uint32_t *bundleId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleInitFromMemImpl(const void* model, size_t modelSize, void *varWeightPtr,
                                                          size_t varWeightSize, uint32_t *bundleId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleLoadModelImpl(uint32_t bundleId, size_t index, uint32_t *modelId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleLoadModelWithMemImpl(uint32_t bundleId, size_t index, void *workPtr,
                                                              size_t workSize, void *weightPtr,
                                                              size_t weightSize, uint32_t *modelId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleLoadModelWithConfigImpl(uint32_t bundleId, size_t index,
                                                                aclmdlConfigHandle *handle, uint32_t *modelId);

ACL_FUNC_VISIBILITY aclError aclmdlBundleUnloadModelImpl(uint32_t bundleId, uint32_t modelId);

ACL_FUNC_VISIBILITY aclError aclmdlLoadFromMemImpl(const void *model,  size_t modelSize, uint32_t *modelId);

ACL_FUNC_VISIBILITY aclError aclmdlLoadFromFileWithMemImpl(const char *modelPath,
                                                       uint32_t *modelId, void *workPtr, size_t workSize,
                                                       void *weightPtr, size_t weightSize);

ACL_FUNC_VISIBILITY aclError aclmdlLoadFromMemWithMemImpl(const void *model, size_t modelSize,
                                                      uint32_t *modelId, void *workPtr, size_t workSize,
                                                      void *weightPtr, size_t weightSize);

ACL_FUNC_VISIBILITY aclError aclmdlLoadFromFileWithQImpl(const char *modelPath, uint32_t *modelId, const uint32_t *inputQ,
                                                     size_t inputQNum, const uint32_t *outputQ, size_t outputQNum);

ACL_FUNC_VISIBILITY aclError aclmdlLoadFromMemWithQImpl(const void *model, size_t modelSize, uint32_t *modelId,
                                                    const uint32_t *inputQ, size_t inputQNum,
                                                    const uint32_t *outputQ, size_t outputQNum);

ACL_FUNC_VISIBILITY aclError aclmdlExecuteImpl(uint32_t modelId, const aclmdlDataset *input, aclmdlDataset *output);

ACL_FUNC_VISIBILITY aclError aclmdlExecuteV2Impl(uint32_t modelId, const aclmdlDataset *input, aclmdlDataset *output,
                                             aclrtStream stream, const aclmdlExecConfigHandle *handle);

ACL_FUNC_VISIBILITY  aclError aclmdlExecuteAsyncV2Impl(uint32_t modelId, const aclmdlDataset *input, aclmdlDataset *output,
                                                   aclrtStream stream, const aclmdlExecConfigHandle *handle);

ACL_FUNC_VISIBILITY aclError aclmdlExecuteAsyncImpl(uint32_t modelId, const aclmdlDataset *input,
                                                aclmdlDataset *output, aclrtStream stream);

ACL_FUNC_VISIBILITY aclError aclmdlUnloadImpl(uint32_t modelId);

ACL_FUNC_VISIBILITY aclError aclmdlQuerySizeImpl(const char *fileName, size_t *workSize, size_t *weightSize);

ACL_FUNC_VISIBILITY aclError aclmdlQueryExeOMDescImpl(const char *fileName, aclmdlExeOMDesc *mdlPartitionSize);

ACL_FUNC_VISIBILITY aclError aclmdlQuerySizeFromMemImpl(const void *model, size_t modelSize, size_t *workSize,
                                                    size_t *weightSize);

ACL_FUNC_VISIBILITY aclError aclmdlSetDynamicBatchSizeImpl(uint32_t modelId, aclmdlDataset *dataset, size_t index,
                                                       uint64_t batchSize);

ACL_FUNC_VISIBILITY aclError aclmdlSetDynamicHWSizeImpl(uint32_t modelId, aclmdlDataset *dataset, size_t index,
                                                    uint64_t height, uint64_t width);

ACL_FUNC_VISIBILITY aclError aclmdlSetInputDynamicDimsImpl(uint32_t modelId, aclmdlDataset *dataset, size_t index,
                                                       const aclmdlIODims *dims);

ACL_FUNC_VISIBILITY aclError aclmdlGetCurOutputDimsImpl(const aclmdlDesc *modelDesc, size_t index, aclmdlIODims *dims);

ACL_FUNC_VISIBILITY const char *aclmdlGetOpAttrImpl(aclmdlDesc *modelDesc, const char *opName, const char *attr);

ACL_FUNC_VISIBILITY aclError aclmdlSetInputAIPPImpl(uint32_t modelId,
                                                aclmdlDataset *dataset,
                                                size_t index,
                                                const aclmdlAIPP *aippParmsSet);

ACL_FUNC_VISIBILITY aclError aclmdlSetAIPPByInputIndexImpl(uint32_t modelId,
                                                       aclmdlDataset *dataset,
                                                       size_t index,
                                                       const aclmdlAIPP *aippParmsSet);

ACL_FUNC_VISIBILITY aclError aclmdlGetAippTypeImpl(uint32_t modelId,
                                               size_t index,
                                               aclmdlInputAippType *type,
                                               size_t *dynamicAttachedDataIndex);

ACL_FUNC_VISIBILITY aclError aclmdlGetFirstAippInfoImpl(uint32_t modelId, size_t index, aclAippInfo *aippInfo);

ACL_FUNC_VISIBILITY aclError aclmdlCreateAndGetOpDescImpl(uint32_t deviceId, uint32_t streamId,
    uint32_t taskId, char *opName, size_t opNameLen, aclTensorDesc **inputDesc, size_t *numInputs,
    aclTensorDesc **outputDesc, size_t *numOutputs);

ACL_FUNC_VISIBILITY aclError aclmdlInitDumpImpl();

ACL_FUNC_VISIBILITY aclError aclmdlSetDumpImpl(const char *dumpCfgPath);

ACL_FUNC_VISIBILITY aclError aclmdlFinalizeDumpImpl();

ACL_FUNC_VISIBILITY aclError aclmdlLoadWithConfigImpl(const aclmdlConfigHandle *handle, uint32_t *modelId);

ACL_FUNC_VISIBILITY aclError aclRecoverAllHcclTasksImpl(int32_t deviceId);


ACL_FUNC_VISIBILITY aclError aclTransTensorDescFormatImpl(const aclTensorDesc *srcDesc, aclFormat dstFormat,
                                                      aclTensorDesc **dstDesc);


#ifdef __cplusplus
}
#endif

#endif // ACL_MODEL_SRC_MODEL_ACL_MODEL_IMPL_H_
