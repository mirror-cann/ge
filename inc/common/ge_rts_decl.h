/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_INC_COMMON_GE_RTS_DECL_H_
#define GE_INC_COMMON_GE_RTS_DECL_H_

// 这里的rt接口声明用于日落的GE代码使用，待GE代码日落时同时删除这些声明，新增代码禁止使用这里的定义。

#include <cstdint>
#include "rt_external_kernel.h"
#include "rt_external_stars.h"
#include "rt_external_mem.h"
#include "rt_external_ffts_define.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct rtFunctionInfo {
  void *pcAddr;
  uint32_t prefetchCnt;
  uint8_t mixType;
  uint8_t reserved[3];
} rtFunctionInfo_t;

typedef struct tagRtKernelInfo {
  uint8_t functionInfoNum;
  uint8_t reserved[3];
  rtFunctionInfo_t functionInfo[2];
} rtKernelDetailInfo_t;

#define RT_DYNAMIC_SHAPE_KERNEL (0x01U)
#define RT_STATIC_SHAPE_KERNEL (0x00U)

#define RT_KERNEL_DEVICE_FIRST (0x10U)
#define RT_KERNEL_HOST_ONLY (0x20U)
#define RT_KERNEL_HOST_FIRST (0x40U)

#define FUNC_MODE_NORMAL (0U)

typedef rtSmDesc_t rtL2Ctrl_t;

typedef struct {
    uint32_t addrOffset;
    uint32_t dataOffset;
} rtPlaceHolderInfo_t;

typedef struct {
    rtAicpuArgsEx_t baseArgs;
    size_t cpuParamHeadOffset;
    uint32_t rsv[4];
} rtCpuKernelArgs_t;

typedef enum tagRtMemRequestFeature {
  MEM_REQUEST_FEATURE_DEFAULT = 0,
  MEM_REQUEST_FEATURE_OPP,
  MEM_REQUEST_FEATURE_RESERVED
} rtMemRequestFeature_t;

#define RT_MQ_QUERY_QUES_ATTR_ENTITY_TYPE ((rtMemQueueQueryCmd_t)2)

typedef struct tagNodeInfo_t {
    uint32_t nodeIdx;
    uint32_t reserved[1];
} rtNodeInfo;

typedef struct tagHwtsInfo_t {
    uint16_t taskId;
    uint16_t sqExeHead;
    uint16_t streamExeHead;
    uint16_t reserved[2];
} rtHwtsInfo;

typedef struct tagLabelDevInfo_t {
    uint16_t modelId;
    uint16_t streamId;
    uint16_t labelId;
    union {
        rtNodeInfo nodeInfo;
        rtHwtsInfo hwtsInfo;
        uint16_t reserved[5];
    }u;
} rtLabelDevInfo;

#define RT_STREAM_FAST_LAUNCH (0x200U)
#define RT_STREAM_FAST_SYNC   (0x400U)

typedef void (*rtCallback_t)(void *fnData);

typedef enum rtKernelType {
    KERNEL_TYPE_CCE = 0,
    KERNEL_TYPE_FWK = 1,
    KERNEL_TYPE_AICPU = 2,
    KERNEL_TYPE_AICPU_CUSTOM = 4,
    KERNEL_TYPE_AICPU_KFC = 5,
    KERNEL_TYPE_CUSTOM_KFC = 6,
    KERNEL_TYPE_HWTS = 10,
    KERNEL_TYPE_RESERVED = 99,
} rtKernelType_t;

#define RT_CAPABILITY_SUPPORT (0x1U)

RTS_API rtError_t rtKernelLaunchWithHandleV2(void *hdl, const uint64_t tilingKey, uint32_t numBlocks,
    rtArgsEx_t *argsInfo, rtSmDesc_t *smDesc, rtStream_t stm, const rtTaskCfgInfo_t *cfgInfo);

RTS_API rtError_t rtVectorCoreKernelLaunchWithHandle(void *hdl, const uint64_t tilingKey, uint32_t numBlocks,
    rtArgsEx_t *argsInfo, rtSmDesc_t *smDesc, rtStream_t stm, const rtTaskCfgInfo_t *cfgInfo);

RTS_API rtError_t rtVectorCoreKernelLaunch(const void *stubFunc, uint32_t numBlocks, rtArgsEx_t *argsInfo,
    rtSmDesc_t *smDesc, rtStream_t stm, uint32_t flags, const rtTaskCfgInfo_t *cfgInfo);

RTS_API rtError_t rtGetSocVersion(char_t *ver, const uint32_t maxLen);

RTS_API rtError_t rtMemcpyAsync(void *dst, uint64_t destMax, const void *src, uint64_t cnt, rtMemcpyKind_t kind,
    rtStream_t stm);

RTS_API rtError_t rtKernelLaunchWithFlagV2(const void *stubFunc, uint32_t numBlocks, rtArgsEx_t *argsInfo,
    rtSmDesc_t *smDesc, rtStream_t stm, uint32_t flags, const rtTaskCfgInfo_t *cfgInfo);

RTS_API rtError_t rtKernelLaunchEx(void *args, uint32_t argsSize, uint32_t flags, rtStream_t stm);

RTS_API rtError_t rtKernelLaunchFwk(const char_t *opName, void *args, uint32_t argsSize, uint32_t flags,
    rtStream_t rtStream);

RTS_API rtError_t rtAicpuKernelLaunchWithFlag(const rtKernelLaunchNames_t *launchNames, uint32_t numBlocks,
    const rtArgsEx_t *argsInfo, rtSmDesc_t *smDesc, rtStream_t stm, uint32_t flags);

RTS_API rtError_t rtDevBinaryRegister(const rtDevBinary_t *bin, void **hdl);

RTS_API rtError_t rtDevBinaryUnRegister(void *hdl);

RTS_API rtError_t rtFunctionRegister(void *binHandle, const void *stubFunc, const char_t *stubName,
    const void *kernelInfoExt, uint32_t funcMode);

RTS_API rtError_t rtGetFunctionByName(const char_t *stubName, void **stubFunc);

RTS_API rtError_t rtKernelGetAddrAndPrefCnt(void *hdl, const uint64_t tilingKey, const void * const stubFunc,
    const uint32_t flag, void **addr, uint32_t *prefetchCnt);

RTS_API rtError_t rtKernelGetAddrAndPrefCntV2(void *hdl, const uint64_t tilingKey, const void * const stubFunc,
    const uint32_t flag, rtKernelDetailInfo_t *kernelInfo);

RTS_API rtError_t rtQueryFunctionRegistered(const char_t *stubName);

RTS_API uint32_t rtGetTsMemType(rtMemRequestFeature_t featureType, uint32_t memSize);

RTS_API rtError_t rtCmoAddrTaskLaunch(void *cmoAddrInfo, uint64_t destMax, rtCmoOpCode_t cmoOpCode,
    rtStream_t stm, uint32_t flag);

RTS_API rtError_t rtGetC2cCtrlAddr(uint64_t *addr, uint32_t *len);

#if defined(__cplusplus)
}
#endif

#endif  // GE_INC_COMMON_GE_RTS_DECL_H_
