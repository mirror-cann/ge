/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_BASE_COMMON_GE_RTS_DECL_H_
#define GE_BASE_COMMON_GE_RTS_DECL_H_

// 这里的rt接口声明用于日落的GE代码使用，待GE代码日落时同时删除这些声明。

#include <cstdint>

// 避免出现runtime内部头文件重复定义错误
#ifndef CCE_RUNTIME_KERNEL_H

#include "runtime/rt_external_stars.h"

typedef struct rtFunctionInfo {
  void *pcAddr;
  uint32_t prefetchCnt;
  uint8_t mixType;                  // 0:NO_MIX; 1:MIX_AIC; 2:MIX_AIV; 3:MIX_AIC_AIV
  uint8_t reserved[3];
} rtFunctionInfo_t;

typedef struct tagRtKernelInfo {
  uint8_t functionInfoNum;
  uint8_t reserved[3];
  rtFunctionInfo_t functionInfo[2];
} rtKernelDetailInfo_t;

#define RT_DYNAMIC_SHAPE_KERNEL (0x01U)
#define RT_STATIC_SHAPE_KERNEL (0x00U)

#endif

#if defined(__cplusplus)
extern "C" {
#endif

RTS_API rtError_t rtKernelLaunchWithHandleV2(void *hdl, uint64_t tilingKey, uint32_t numBlocks,
    rtArgsEx_t *argsInfo, rtSmDesc_t *smDesc, rtStream_t stm, const rtTaskCfgInfo_t *cfgInfo);

RTS_API rtError_t rtKernelLaunchWithFlagV2(const void *stubFunc, uint32_t numBlocks, rtArgsEx_t *argsInfo,
    rtSmDesc_t *smDesc, rtStream_t stm, uint32_t flags, const rtTaskCfgInfo_t *cfgInfo);

RTS_API rtError_t rtKernelLaunchEx(void *args, uint32_t argsSize, uint32_t flags, rtStream_t stm);

RTS_API rtError_t rtKernelLaunchFwk(const char_t *opName, void *args, uint32_t argsSize,
    uint32_t flags, rtStream_t stm);

RTS_API rtError_t rtAicpuKernelLaunchWithFlag(const rtKernelLaunchNames_t *launchNames, uint32_t numBlocks,
    const rtArgsEx_t *argsInfo, rtSmDesc_t *smDesc, rtStream_t stm, uint32_t flags);

RTS_API rtError_t rtDevBinaryUnRegister(void *hdl);

RTS_API rtError_t rtMetadataRegister(void *hdl, const char_t *metadata);

RTS_API rtError_t rtFunctionRegister(void *binHandle, const void *stubFunc, const char_t *stubName,
    const void *kernelInfoExt, uint32_t funcMode);

RTS_API rtError_t rtGetFunctionByName(const char_t *stubName, void **stubFunc);

RTS_API rtError_t rtKernelGetAddrAndPrefCntV2(void *hdl, const uint64_t tilingKey, const void * const stubFunc,
                                              const uint32_t flag, rtKernelDetailInfo_t *kernelInfo);

#ifndef CCE_RUNTIME_DEVICE_H
typedef enum tagRtMemRequestFeature {
  MEM_REQUEST_FEATURE_DEFAULT = 0,
  MEM_REQUEST_FEATURE_OPP,
  MEM_REQUEST_FEATURE_RESERVED
} rtMemRequestFeature_t;

RTS_API uint32_t rtGetTsMemType(rtMemRequestFeature_t featureType, uint32_t memSize);
#endif

#if defined(__cplusplus)
}
#endif

#endif  // GE_BASE_COMMON_GE_RTS_DECL_H_
