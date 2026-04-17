/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "adapter_dlhcclfunc.h"

HcclResult HcceAllReduce(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
                         HcclComm comm, aclrtStream stream) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclAllReduce(sendBuf, recvBuf, count, dataType, op, comm, stream);
}

HcclResult HcceBroadcast(void *buf, uint64_t count, HcclDataType dataType, uint32_t root, HcclComm comm,
                         aclrtStream stream) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclBroadcast(buf, count, dataType, root, comm, stream);
}

HcclResult HcceReduceScatter(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType, HcclReduceOp op,
                             HcclComm comm, aclrtStream stream) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclReduceScatter(sendBuf, recvBuf, recvCount, dataType, op, comm, stream);
}

HcclResult HcceReduceScatterV(void *sendBuf, const void *sendCounts, const void *sendDispls, void *recvBuf,
                              uint64_t recvCount, HcclDataType dataType, HcclReduceOp op, HcclComm comm,
                              aclrtStream stream) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclReduceScatterV(sendBuf, sendCounts, sendDispls, recvBuf, recvCount,
                                                             dataType, op, comm, stream);
}

HcclResult HcceAllGather(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType, HcclComm comm,
                         aclrtStream stream) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclAllGather(sendBuf, recvBuf, sendCount, dataType, comm, stream);
}

HcclResult HcceAllGatherV(void *sendBuf, uint64_t sendCount, void *recvBuf, const void *recvCounts,
                          const void *recvDispls, HcclDataType dataType, HcclComm comm, aclrtStream stream) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, dataType,
                                                         comm, stream);
}

HcclResult HcceSend(void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t destRank, HcclComm comm,
                    aclrtStream stream) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclSend(sendBuf, count, dataType, destRank, comm, stream);
}

HcclResult HcceRecv(void *recvBuf, uint64_t count, HcclDataType dataType, uint32_t srcRank, HcclComm comm,
                    aclrtStream stream) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclRecv(recvBuf, count, dataType, srcRank, comm, stream);
}

HcclResult HcceAlltoAllVC(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType, const void *recvBuf,
                          HcclDataType recvType, HcclComm comm, aclrtStream stream) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclAlltoAllVC(sendBuf, sendCountMatrix, sendType, recvBuf, recvType, comm,
                                                         stream);
}

HcclResult HcceAlltoAllV(const void *sendBuf, const void *sendCounts, const void *sdispls, HcclDataType sendType,
                         const void *recvBuf, const void *recvCounts, const void *rdispls, HcclDataType recvType,
                         HcclComm comm, aclrtStream stream) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclAlltoAllV(sendBuf, sendCounts, sdispls, sendType, recvBuf, recvCounts,
                                                        rdispls, recvType, comm, stream);
}

HcclResult HcceAlltoAll(const void *sendBuf, uint64_t sendCount, HcclDataType sendType, const void *recvBuf,
                        uint64_t recvCount, HcclDataType recvType, HcclComm comm, aclrtStream stream) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclAlltoAll(sendBuf, sendCount, sendType, recvBuf, recvCount, recvType, comm,
                                                       stream);
}

HcclResult HcceReduce(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
                      uint32_t root, HcclComm comm, aclrtStream stream) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclReduce(sendBuf, recvBuf, count, dataType, op, root, comm, stream);
}

HcclResult HcceGetandClearOverFlowTasks(const char *group, hccl::HcclDumpInfo **hcclDumpInfo, s32 *len) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcomGetandClearOverFlowTasks(group, hcclDumpInfo, len);
}

// 图模式相关函数实现
HcclResult HcceIsHcclGraphModeValid(bool &isValid) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);
  
  isValid = DlHcclFunction::get_instance().isLoadHcclGraphModeFunctions();
  return HCCL_SUCCESS;
}

HcclResult HcceCreateOpParamGraphMode(OpParamGraphModePtr *opParam) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclCreateOpParamGraphMode(opParam);
}

HcclResult HcceDestroyOpParamGraphMode(OpParamGraphModePtr opParam) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclDestroyOpParamGraphMode(opParam);
}

HcclResult HcceSetOpParamGraphModeOpType(OpParamGraphModePtr opParam, const char *opType) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclSetOpParamGraphModeOpType(opParam, opType);
}

HcclResult HcceSetOpParamGraphModeDataCount(OpParamGraphModePtr opParam, const u64 *dataCount) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclSetOpParamGraphModeDataCount(opParam, dataCount);
}

HcclResult HcceSetOpParamGraphModeRankSize(OpParamGraphModePtr opParam, const u32 *rankSize) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclSetOpParamGraphModeRankSize(opParam, rankSize);
}

HcclResult HcceSetOpParamGraphModeHCCLBufferSize(OpParamGraphModePtr opParam, const u64 *hcclBufferSize) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclSetOpParamGraphModeHCCLBufferSize(opParam, hcclBufferSize);
}

HcclResult HcceSetOpParamGraphModeDataType(OpParamGraphModePtr opParam, HcclDataType dataType) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclSetOpParamGraphModeDataType(opParam, dataType);
}

HcclResult HcceSetAivSelectOpParamGraphMode(OpParamGraphModePtr opParam, const char *group, u64 count, void *counts, 
                                      HcclDataType dataType, HcclReduceOp op, HcclCMDType opTypeAiv, u32 aivCoreLimit, bool ifAiv) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclSetAivSelectOpParamGraphMode(opParam, group, count, counts, dataType, op, opTypeAiv, aivCoreLimit, ifAiv);
}

HcclResult HcceCalcOpResOfflineGraphMode(OpParamGraphModePtr opParam, u64 *opMemSize, u32 *streamNum, u32 *taskNum, u32 *aivCoreNum) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclCalcOpResOfflineGraphMode(opParam, opMemSize, streamNum, taskNum, aivCoreNum);
}

HcclResult HcceCalcOpResOnlineGraphMode(OpParamGraphModePtr opParam, u64 *opMemSize, u32 *streamNum, u32 *taskNum, u32 *aivCoreNum) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);

  return DlHcclFunction::get_instance().dlHcclCalcOpResOnlineGraphMode(opParam, opMemSize, streamNum, taskNum, aivCoreNum);
}

HcclResult HcceAllGatherGraphMode(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType, const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
                HCCL_E_PARA);
    return DlHcclFunction::get_instance().dlHcclAllGatherGraphMode(sendBuf, recvBuf, sendCount, dataType, group, stream, tag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult HcceBroadcastGraphMode(void *buf, uint64_t count, HcclDataType dataType, uint32_t root, const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
                HCCL_E_PARA);
    return DlHcclFunction::get_instance().dlHcclBroadcastGraphMode(buf, count, dataType, root, group, stream, tag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult HcceReduceScatterVGraphMode(void *sendBuf, const void *sendCounts, const void *sendDispls, void *recvBuf, uint64_t recvCount, HcclDataType dataType, HcclReduceOp op, const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
                HCCL_E_PARA);
    return DlHcclFunction::get_instance().dlHcclReduceScatterVGraphMode(sendBuf, sendCounts, sendDispls, recvBuf, recvCount, dataType, op, group, stream, tag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult HcceAllGatherVGraphMode(void *sendBuf, void *recvBuf, uint64_t sendCount, const void *recvCounts, const void *recvDispls, HcclDataType dataType, const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
                HCCL_E_PARA);
    return DlHcclFunction::get_instance().dlHcclAllGatherVGraphMode(sendBuf, recvBuf, sendCount, recvCounts, recvDispls, dataType, group, stream, tag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult HcceAlltoAllGraphMode(const void *sendBuf, uint64_t sendCount, HcclDataType sendType, const void *recvBuf, uint64_t recvCount, HcclDataType recvType, 
  const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
                HCCL_E_PARA);
    return DlHcclFunction::get_instance().dlHcclAlltoAllGraphMode(sendBuf, sendCount, sendType, recvBuf, recvCount, recvType,
      group, stream, tag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult HcceAlltoAllVGraphMode(const void *sendBuf, const void *sendCounts, const void *sdispls, HcclDataType sendType,
  const void *recvBuf, const void *recvCounts, const void *rdispls, HcclDataType recvType,
  const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
                HCCL_E_PARA);
    return DlHcclFunction::get_instance().dlHcclAlltoAllVGraphMode(sendBuf, sendCounts, sdispls, sendType, recvBuf, recvCounts, rdispls, recvType,
      group, stream, tag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult HcceAlltoAllVCGraphMode(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType, const void *recvBuf, HcclDataType recvType,
  const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
                HCCL_E_PARA);
    return DlHcclFunction::get_instance().dlHcclAlltoAllVCGraphMode(sendBuf, sendCountMatrix, sendType, recvBuf, recvType,
      group, stream, tag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult HcceSendGraphMode(
    void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t destRank, const char* group, aclrtStream stream,
    const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize)
{
    CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
                HCCL_E_PARA);
    return DlHcclFunction::get_instance().dlHcclSendGraphMode(sendBuf, count, dataType, destRank, group, stream, tag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult HcceRecvGraphMode(
    void *recvBuf, uint64_t count, HcclDataType dataType, uint32_t srcRank, const char* group, aclrtStream stream,
    const char* tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize)
{
    CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
                HCCL_E_PARA);
    return DlHcclFunction::get_instance().dlHcclRecvGraphMode(recvBuf, count, dataType, srcRank, group, stream, tag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult HcceAllReduceGraphMode(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType, 
                                  HcclReduceOp op, const char *group, void *stream, const char *tag, void **streams, 
                                  size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) 
{
    CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
                HCCL_E_PARA);
    return DlHcclFunction::get_instance().dlHcclAllReduceGraphMode(sendBuf, recvBuf, sendCount, dataType, op, group, stream, tag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult HcceReduceGraphMode(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op, uint32_t root,
                               const char *group, void *stream, const char *tag, void **streams, size_t streamCount,
                               void *scratchMemAddr, uint64_t scratchMemSize) {
  CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
              HCCL_E_PARA);
  return DlHcclFunction::get_instance().dlHcclReduceGraphMode(sendBuf, recvBuf, count, dataType, op, root, group, stream, tag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult HcceReduceScatterGraphMode(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType,
                                       HcclReduceOp op, const char *group, void *stream, const char *tag, void **streams,
                                       size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize)
{
    CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
                HCCL_E_PARA);
    return DlHcclFunction::get_instance().dlHcclReduceScatterGraphMode(sendBuf, recvBuf, recvCount, dataType, op,
                                                                       group, stream, tag, streams, streamCount,
                                                                       scratchMemAddr, scratchMemSize);
}
HcclResult HcceSetAivCoreLimitGraphMode(const char *group, u32 aivCoreLimit)
{
    CHK_PRT_RET(DlHcclFunction::get_instance().init() != HCCL_SUCCESS, HCCL_ERROR("DlHcclFunction::get_instance().init() fail \n"),
                HCCL_E_PARA);
    return DlHcclFunction::get_instance().dlHcclSetAivCoreLimitGraphMode(group, aivCoreLimit);
}
