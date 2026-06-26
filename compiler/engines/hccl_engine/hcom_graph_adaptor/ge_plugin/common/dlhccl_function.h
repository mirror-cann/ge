/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DLHCCL_FUNCTION_H
#define DLHCCL_FUNCTION_H

#include <mutex>
#include <dlfcn.h>
#include <functional>
#include <memory>
#include "hccl/hccl_types.h"
#include "hccl/hcom.h"
#include "hcom_log.h"

using aclrtStream = void *;

// 因为ge中不感知OpParamGraphMode，所以使用void*
using OpParamGraphModePtr = void *;

class DlHcclFunction {
 public:
  static DlHcclFunction &get_instance();
  HcclResult init();
  void deinit();
  bool isLoadHcclGraphModeFunctions();
  HcclResult dlHcclAllReduce(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
                             HcclComm comm, aclrtStream stream);

  HcclResult dlHcclBroadcast(void *buf, uint64_t count, HcclDataType dataType, uint32_t root, HcclComm comm,
                             aclrtStream stream);

  HcclResult dlHcclReduceScatter(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType,
                                 HcclReduceOp op, HcclComm comm, aclrtStream stream);

  HcclResult dlHcclReduceScatterV(void *sendBuf, const void *sendCounts, const void *sendDispls, void *recvBuf,
                                  uint64_t recvCount, HcclDataType dataType, HcclReduceOp op, HcclComm comm,
                                  aclrtStream stream);

  HcclResult dlHcclAllGather(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType, HcclComm comm,
                             aclrtStream stream);

  HcclResult dlHcclAllGatherV(void *sendBuf, uint64_t sendCount, void *recvBuf, const void *recvCounts,
                              const void *recvDispls, HcclDataType dataType, HcclComm comm, aclrtStream stream);

  HcclResult dlHcclSend(void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t destRank, HcclComm comm,
                        aclrtStream stream);

  HcclResult dlHcclRecv(void *recvBuf, uint64_t count, HcclDataType dataType, uint32_t srcRank, HcclComm comm,
                        aclrtStream stream);

  HcclResult dlHcclAlltoAllVC(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType,
                              const void *recvBuf, HcclDataType recvType, HcclComm comm, aclrtStream stream);

  HcclResult dlHcclAlltoAllV(const void *sendBuf, const void *sendCounts, const void *sdispls, HcclDataType sendType,
                             const void *recvBuf, const void *recvCounts, const void *rdispls, HcclDataType recvType,
                             HcclComm comm, aclrtStream stream);

  HcclResult dlHcclAlltoAll(const void *sendBuf, uint64_t sendCount, HcclDataType sendType, const void *recvBuf,
                            uint64_t recvCount, HcclDataType recvType, HcclComm comm, aclrtStream stream);

  HcclResult dlHcclReduce(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
                          uint32_t root, HcclComm comm, aclrtStream stream);

  HcclResult dlHcomGetandClearOverFlowTasks(const char *group, hccl::HcclDumpInfo **hcclDumpInfoPtr, s32 *len);

  // 图模式相关函数
  HcclResult dlHcclCreateOpParamGraphMode(OpParamGraphModePtr *opParam);
  HcclResult dlHcclDestroyOpParamGraphMode(OpParamGraphModePtr opParam);
  HcclResult dlHcclSetOpParamGraphModeOpType(OpParamGraphModePtr opParam, const char *opType);
  HcclResult dlHcclSetOpParamGraphModeDataCount(OpParamGraphModePtr opParam, const u64 *dataCount);
  HcclResult dlHcclSetOpParamGraphModeDataType(OpParamGraphModePtr opParam, const HcclDataType dataType);
  HcclResult dlHcclSetOpParamGraphModeRankSize(OpParamGraphModePtr opParam, const u32 *rankSize);
  HcclResult dlHcclSetOpParamGraphModeHCCLBufferSize(OpParamGraphModePtr opParam, const u64 *hcclBufferSize);
  HcclResult dlHcclSetAivSelectOpParamGraphMode(OpParamGraphModePtr opParam, u32 aivCoreLimit);
  HcclResult dlHcclCalcOpResOfflineGraphMode(OpParamGraphModePtr opParam, u64 *opMemSize, u32 *streamNum, u32 *taskNum,
                                             u32 *aivCoreNum);
  HcclResult dlHcclCalcOpResOnlineGraphMode(OpParamGraphModePtr opParam, u64 *opMemSize, u32 *streamNum, u32 *taskNum,
                                            u32 *aivCoreNum);
  HcclResult dlHcclAllGatherGraphMode(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType,
                                      const char *group, void *stream, const char *optag, void **streams,
                                      size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);
  HcclResult dlHcclBroadcastGraphMode(void *buf, uint64_t count, HcclDataType dataType, uint32_t root,
                                      const char *group, void *stream, const char *optag, void **streams,
                                      size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);
  HcclResult dlHcclReduceScatterVGraphMode(void *sendBuf, const void *sendCounts, const void *sendDispls, void *recvBuf,
                                           uint64_t recvCount, HcclDataType dataType, HcclReduceOp op,
                                           const char *group, void *stream, const char *optag, void **streams,
                                           size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);
  HcclResult dlHcclAllGatherVGraphMode(void *sendBuf, void *recvBuf, uint64_t sendCount, const void *recvCounts,
                                       const void *recvDispls, HcclDataType dataType, const char *group, void *stream,
                                       const char *optag, void **streams, size_t streamCount, void *scratchMemAddr,
                                       uint64_t scratchMemSize);
  HcclResult dlHcclAlltoAllGraphMode(const void *sendBuf, uint64_t sendCount, HcclDataType sendType,
                                     const void *recvBuf, uint64_t recvCount, HcclDataType recvType, const char *group,
                                     void *stream, const char *optag, void **streams, size_t streamCount,
                                     void *scratchMemAddr, uint64_t scratchMemSize);
  HcclResult dlHcclAlltoAllVGraphMode(const void *sendBuf, const void *sendCounts, const void *sdispls,
                                      HcclDataType sendType, const void *recvBuf, const void *recvCounts,
                                      const void *rdispls, HcclDataType recvType, const char *group, void *stream,
                                      const char *optag, void **streams, size_t streamCount, void *scratchMemAddr,
                                      uint64_t scratchMemSize);
  HcclResult dlHcclAlltoAllVCGraphMode(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType,
                                       const void *recvBuf, HcclDataType recvType, const char *group, void *stream,
                                       const char *optag, void **streams, size_t streamCount, void *scratchMemAddr,
                                       uint64_t scratchMemSize);
  HcclResult dlHcclSendGraphMode(void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t destRank,
                                 const char *group, void *stream, const char *tag, void **streams, size_t streamCount,
                                 void *scratchMemAddr, uint64_t scratchMemSize);
  HcclResult dlHcclRecvGraphMode(void *recvBuf, uint64_t count, HcclDataType dataType, uint32_t srcRank,
                                 const char *group, void *stream, const char *tag, void **streams, size_t streamCount,
                                 void *scratchMemAddr, uint64_t scratchMemSize);
  HcclResult dlHcclAllReduceGraphMode(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType,
                                      HcclReduceOp op, const char *group, void *stream, const char *optag,
                                      void **streams, size_t streamCount, void *scratchMemAddr,
                                      uint64_t scratchMemSize);
  HcclResult dlHcclReduceGraphMode(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
                                   uint32_t root, const char *group, void *stream, const char *optag, void **streams,
                                   size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize);
  HcclResult dlHcclReduceScatterGraphMode(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType,
                                          HcclReduceOp op, const char *group, void *stream, const char *optag,
                                          void **streams, size_t streamCount, void *scratchMemAddr,
                                          uint64_t scratchMemSize);
  HcclResult dlHcclSetAivCoreLimitGraphMode(const char *group, u32 aivCoreLimit);
  HcclResult dlHcclSelectAlgGraphMode(const char *group, u64 count, HcclDataType dataType, HcclReduceOp op,
                                      HcclCMDType opType, int32_t aivCoreLimit, bool *ifAiv, char *algName);
  HcclResult dlHcclCalcAivCoreNumGraphMode(u32 aivCoreLimit, u32 *blockDim);
  HcclResult dlHcclGetAlgExecParamGraphMode(const char *tag, const char *group, u64 count, void *inputPtr,
                                            void *outputPtr, HcclCMDType opType, bool clearEnable,
                                            HcclDataType dataType, HcclReduceOp op, void **commContext, u64 *len,
                                            u32 aivCoreLimit);

 private:
  DlHcclFunction();
  ~DlHcclFunction();
  DlHcclFunction(const DlHcclFunction &) = delete;
  DlHcclFunction &operator=(const DlHcclFunction &) = delete;

  void *dl_hccl_handle;
  void *dl_hcomm_handle;
  std::mutex handleMutex_;
  bool isHcclGraphModeFunctionsLoaded_ = false;
  HcclResult initHcclGraphModeFunctions();
  std::function<HcclResult(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType, HcclComm comm,
                           aclrtStream stream)>
      dlHcclAllGatherFunc;

  std::function<HcclResult(void *sendBuf, uint64_t sendCount, void *recvBuf, const void *recvCounts,
                           const void *recvDispls, HcclDataType dataType, HcclComm comm, aclrtStream stream)>
      dlHcclAllGatherVFunc;

  std::function<HcclResult(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
                           HcclComm comm, aclrtStream stream)>
      dlHcclAllReduceFunc;

  std::function<HcclResult(void *buf, uint64_t count, HcclDataType dataType, uint32_t root, HcclComm comm,
                           aclrtStream stream)>
      dlHcclBroadcastFunc;

  std::function<HcclResult(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType, HcclReduceOp op,
                           HcclComm comm, aclrtStream stream)>
      dlHcclReduceScatterFunc;

  std::function<HcclResult(void *sendBuf, const void *sendCounts, const void *sendDispls, void *recvBuf,
                           uint64_t recvCount, HcclDataType dataType, HcclReduceOp op, HcclComm comm,
                           aclrtStream stream)>
      dlHcclReduceScatterVFunc;

  std::function<HcclResult(const void *sendBuf, const void *sendCounts, const void *sdispls, HcclDataType sendType,
                           const void *recvBuf, const void *recvCounts, const void *rdispls, HcclDataType recvType,
                           HcclComm comm, aclrtStream stream)>
      dlHcclAlltoAllVFunc;

  std::function<HcclResult(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType, const void *recvBuf,
                           HcclDataType recvType, HcclComm comm, aclrtStream stream)>
      dlHcclAlltoAllVCFunc;

  std::function<HcclResult(const void *sendBuf, uint64_t sendCount, HcclDataType sendType, const void *recvBuf,
                           uint64_t recvCount, HcclDataType recvType, HcclComm comm, aclrtStream stream)>
      dlHcclAlltoAllFunc;

  std::function<HcclResult(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
                           uint32_t root, HcclComm comm, aclrtStream stream)>
      dlHcclReduceFunc;

  std::function<HcclResult(void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t destRank, HcclComm comm,
                           aclrtStream stream)>
      dlHcclSendFunc;

  std::function<HcclResult(void *recvBuf, uint64_t count, HcclDataType dataType, uint32_t srcRank, HcclComm comm,
                           aclrtStream stream)>
      dlHcclRecvFunc;

  std::function<HcclResult(const char *group, hccl::HcclDumpInfo **hcclDumpInfoPtr, s32 *len)>
      dlHcomGetandClearOverFlowTasksFunc;

  // 图模式相关函数指针
  std::function<HcclResult(OpParamGraphModePtr *opParam)> dlHcclCreateOpParamGraphModeFunc;
  std::function<HcclResult(OpParamGraphModePtr opParam)> dlHcclDestroyOpParamGraphModeFunc;
  std::function<HcclResult(OpParamGraphModePtr opParam, const char *opType)> dlHcclSetOpParamGraphModeOpTypeFunc;
  std::function<HcclResult(OpParamGraphModePtr, u32)> dlHcclSetAivSelectOpParamGraphModeFunc;
  std::function<HcclResult(OpParamGraphModePtr, const u64 *)> dlHcclSetOpParamGraphModeDataCountFunc;
  std::function<HcclResult(OpParamGraphModePtr, const u32 *)> dlHcclSetOpParamGraphModeRankSizeFunc;
  std::function<HcclResult(OpParamGraphModePtr, const u64 *)> dlHcclSetOpParamGraphModeHCCLBufferSizeFunc;
  std::function<HcclResult(OpParamGraphModePtr, const HcclDataType)> dlHcclSetOpParamGraphModeDataTypeFunc;
  std::function<HcclResult(OpParamGraphModePtr, u64 *, u32 *, u32 *, u32 *)> dlHcclCalcOpResOfflineGraphModeFunc;
  std::function<HcclResult(OpParamGraphModePtr opParam, u64 *opMemSize, u32 *streamNum, u32 *taskNum, u32 *aivCoreNum)>
      dlHcclCalcOpResOnlineGraphModeFunc;
  std::function<HcclResult(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType, const char *group,
                           void *stream, const char *optag, void **streams, size_t streamCount, void *scratchMemAddr,
                           uint64_t scratchMemSize)>
      dlHcclAllGatherGraphModeFunc;
  std::function<HcclResult(void *buf, uint64_t count, HcclDataType dataType, uint32_t root, const char *group,
                           void *stream, const char *optag, void **streams, size_t streamCount, void *scratchMemAddr,
                           uint64_t scratchMemSize)>
      dlHcclBroadcastGraphModeFunc;
  std::function<HcclResult(void *sendBuf, const void *sendCounts, const void *sendDispls, void *recvBuf,
                           uint64_t recvCount, HcclDataType dataType, HcclReduceOp op, const char *group, void *stream,
                           const char *optag, void **streams, size_t streamCount, void *scratchMemAddr,
                           uint64_t scratchMemSize)>
      dlHcclReduceScatterVGraphModeFunc;
  std::function<HcclResult(void *sendBuf, void *recvBuf, uint64_t sendCount, const void *recvCounts,
                           const void *recvDispls, HcclDataType dataType, const char *group, void *stream,
                           const char *optag, void **streams, size_t streamCount, void *scratchMemAddr,
                           uint64_t scratchMemSize)>
      dlHcclAllGatherVGraphModeFunc;
  std::function<HcclResult(const void *sendBuf, uint64_t sendCount, HcclDataType sendType, const void *recvBuf,
                           uint64_t recvCount, HcclDataType recvType, const char *group, void *stream,
                           const char *optag, void **streams, size_t streamCount, void *scratchMemAddr,
                           uint64_t scratchMemSize)>
      dlHcclAlltoAllGraphModeFunc;
  std::function<HcclResult(const void *sendBuf, const void *sendCounts, const void *sdispls, HcclDataType sendType,
                           const void *recvBuf, const void *recvCounts, const void *rdispls, HcclDataType recvType,
                           const char *group, void *stream, const char *optag, void **streams, size_t streamCount,
                           void *scratchMemAddr, uint64_t scratchMemSize)>
      dlHcclAlltoAllVGraphModeFunc;
  std::function<HcclResult(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType, const void *recvBuf,
                           HcclDataType recvType, const char *group, void *stream, const char *optag, void **streams,
                           size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize)>
      dlHcclAlltoAllVCGraphModeFunc;
  std::function<HcclResult(void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t destRank, const char *group,
                           void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr,
                           uint64_t scratchMemSize)>
      dlHcclSendGraphModeFunc;
  std::function<HcclResult(void *recvBuf, uint64_t count, HcclDataType dataType, uint32_t srcRank, const char *group,
                           void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr,
                           uint64_t scratchMemSize)>
      dlHcclRecvGraphModeFunc;
  std::function<HcclResult(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType, HcclReduceOp op,
                           const char *group, void *stream, const char *optag, void **streams, size_t streamCount,
                           void *scratchMemAddr, uint64_t scratchMemSize)>
      dlHcclAllReduceGraphModeFunc;
  std::function<HcclResult(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
                           uint32_t root, const char *group, void *stream, const char *opTag, void **streams,
                           size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize)>
      dlHcclReduceGraphModeFunc;
  std::function<HcclResult(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType, HcclReduceOp op,
                           const char *group, void *stream, const char *optag, void **streams, size_t streamCount,
                           void *scratchMemAddr, uint64_t scratchMemSize)>
      dlHcclReduceScatterGraphModeFunc;
  std::function<HcclResult(const char *, u32)> dlHcclSetAivCoreLimitGraphModeFunc;
  std::function<HcclResult(const char *, u64, HcclDataType, HcclReduceOp, HcclCMDType, int32_t, bool *, char *)>
      dlHcclSelectAlgGraphModeFunc;
  std::function<HcclResult(u32, u32 *)> dlHcclCalcAivCoreNumGraphModeFunc;
  std::function<HcclResult(const char *, const char *, u64, void *, void *, HcclCMDType, bool, HcclDataType,
                           HcclReduceOp, void **, u64 *, u32)>
      dlHcclGetAlgExecParamGraphModeFunc;
};

#endif
