/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ADAPTER_DLHCCLFUNC_H
#define ADAPTER_DLHCCLFUNC_H

#include "dlhccl_function.h"
#include "hccl/hccl_types.h"
#include "hccl/hcom.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

HcclResult HcceAllReduce(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
                         HcclComm comm, aclrtStream stream);

HcclResult HcceBroadcast(void *buf, uint64_t count, HcclDataType dataType, uint32_t root, HcclComm comm,
                         aclrtStream stream);

HcclResult HcceReduceScatter(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType, HcclReduceOp op,
                             HcclComm comm, aclrtStream stream);

HcclResult HcceReduceScatterV(void *sendBuf, const void *sendCounts, const void *sendDispls, void *recvBuf,
                              uint64_t recvCount, HcclDataType dataType, HcclReduceOp op, HcclComm comm,
                              aclrtStream stream);

HcclResult HcceAllGather(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType, HcclComm comm,
                         aclrtStream stream);

HcclResult HcceAllGatherV(void *sendBuf, uint64_t sendCount, void *recvBuf, const void *recvCounts,
                          const void *recvDispls, HcclDataType dataType, HcclComm comm, aclrtStream stream);

HcclResult HcceSend(void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t destRank, HcclComm comm,
                    aclrtStream stream);

HcclResult HcceRecv(void *recvBuf, uint64_t count, HcclDataType dataType, uint32_t srcRank, HcclComm comm,
                    aclrtStream stream);

HcclResult HcceAlltoAllVC(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType, const void *recvBuf,
                          HcclDataType recvType, HcclComm comm, aclrtStream stream);

HcclResult HcceAlltoAllV(const void *sendBuf, const void *sendCounts, const void *sdispls, HcclDataType sendType,
                         const void *recvBuf, const void *recvCounts, const void *rdispls, HcclDataType recvType,
                         HcclComm comm, aclrtStream stream);

HcclResult HcceAlltoAll(const void *sendBuf, uint64_t sendCount, HcclDataType sendType, const void *recvBuf,
                        uint64_t recvCount, HcclDataType recvType, HcclComm comm, aclrtStream stream);

HcclResult HcceReduce(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
                      uint32_t root, HcclComm comm, aclrtStream stream);

HcclResult HcceGetandClearOverFlowTasks(const char *group, hccl::HcclDumpInfo **hcclDumpInfo, s32 *len);

// 图模式相关函数
// OpParamGraphModePtr 操作函数
HcclResult HcceIsHcclGraphModeValid(bool &isValid);
HcclResult HcceCreateOpParamGraphMode(OpParamGraphModePtr *opParam);
HcclResult HcceDestroyOpParamGraphMode(OpParamGraphModePtr opParam);
HcclResult HcceSetOpParamGraphModeOpType(OpParamGraphModePtr opParam, const char *opType);
// RAII 工具类，用于管理 OpParamGraphModePtr 资源
struct OpParamGraphModeDeleter {
  void operator()(OpParamGraphModePtr ptr) {
    if (ptr != nullptr) {
      HcceDestroyOpParamGraphMode(ptr);
    }
  }
};
// 用于管理 OpParamGraphModePtr 资源的智能指针类型
using OpParamGraphModeGuard = std::unique_ptr<void, OpParamGraphModeDeleter>;
HcclResult HcceSetAivSelectOpParamGraphMode(OpParamGraphModePtr opParam, u32 aivCoreLimit);

// 资源计算函数
HcclResult HcceCalcOpResOfflineGraphMode(OpParamGraphModePtr opParam, u64 *opMemSize, u32 *streamNum, u32 *taskNum, u32 *aivCoreNum);
HcclResult HcceCalcOpResOnlineGraphMode(OpParamGraphModePtr opParam, u64 *opMemSize, u32 *streamNum, u32 *taskNum, u32 *aivCoreNum);
HcclResult HcceSetOpParamGraphModeDataCount(OpParamGraphModePtr opParam, const u64 *dataCount);
HcclResult HcceSetOpParamGraphModeDataType(OpParamGraphModePtr opParam, HcclDataType dataType);
HcclResult HcceSetOpParamGraphModeRankSize(OpParamGraphModePtr opParam, const u32 *rankSize);
HcclResult HcceSetOpParamGraphModeHCCLBufferSize(OpParamGraphModePtr opParam, const u64 *hcclBufferSize);

// 图模式算子函数
HcclResult HcceAllGatherGraphMode(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType, const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);
HcclResult HcceBroadcastGraphMode(void *buf, uint64_t count, HcclDataType dataType, uint32_t root, const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);
HcclResult HcceAllGatherVGraphMode(void *sendBuf, void *recvBuf, uint64_t sendCount, const void *recvCounts, const void *recvDispls, HcclDataType dataType, const char* group, aclrtStream stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);
HcclResult HcceAlltoAllGraphMode(const void *sendBuf, uint64_t sendCount, HcclDataType sendType, const void *recvBuf, uint64_t recvCount, HcclDataType recvType, 
  const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);
HcclResult HcceAlltoAllVGraphMode(const void *sendBuf, const void *sendCounts, const void *sdispls, HcclDataType sendType,
  const void *recvBuf, const void *recvCounts, const void *rdispls, HcclDataType recvType,
  const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);
HcclResult HcceAlltoAllVCGraphMode(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType, const void *recvBuf, HcclDataType recvType,
  const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);

HcclResult HcceReduceScatterVGraphMode(void *sendBuf, const void *sendCounts, const void *sendDispls, void *recvBuf, uint64_t recvCount, HcclDataType dataType, HcclReduceOp op, const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);
HcclResult HcceSendGraphMode(
    void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t destRank, const char* group, aclrtStream stream,
    const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);
HcclResult HcceRecvGraphMode(
    void *recvBuf, uint64_t count, HcclDataType dataType, uint32_t srcRank, const char* group, aclrtStream stream,
    const char* tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);
HcclResult HcceAllReduceGraphMode(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType, HcclReduceOp op, 
                                  const char *group, void *stream, const char *tag, void **streams, size_t streamCount, 
                                  void *scratchMemAddr, uint64_t scratchMemSize);
HcclResult HcceReduceGraphMode(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op, uint32_t root,
                               const char *group, void *stream, const char *tag, void **streams, size_t streamCount,
                               void *scratchMemAddr, uint64_t scratchMemSize);
HcclResult HcceReduceScatterGraphMode(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType,
 	                                    HcclReduceOp op, const char *group, void *stream, const char *tag, void **streams,
 	                                    size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);
HcclResult HcceSelectAlgGraphMode(const char *group, u64 count, HcclDataType dataType, HcclReduceOp op, HcclCMDType opType,
                                  int32_t aivCoreLimit, bool *ifAiv, char *algName);
HcclResult HcceCalcAivCoreNumGraphMode(u32 aivCoreLimit, u32 *blockDim);
HcclResult HcceSetAivCoreLimitGraphMode(const char *group, u32 aivCoreLimit);
HcclResult HcceGetAlgExecParamGraphMode(const char *tag, const char *group, u64 count, void *inputPtr, void *outputPtr, HcclCMDType opType,
      bool clearEnable, HcclDataType dataType, HcclReduceOp op, void **commContext, u64 *len, u32 aivCoreLimit);
#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // ADAPTER_DLHCCL_FUNCTION_H
