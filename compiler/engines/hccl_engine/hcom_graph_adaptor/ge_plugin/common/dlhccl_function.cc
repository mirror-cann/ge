/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <dlfcn.h>
#include "dlhccl_function.h"

DlHcclFunction::DlHcclFunction() : dl_hccl_handle(nullptr), dl_hcomm_handle(nullptr) {}

DlHcclFunction::~DlHcclFunction() {
  deinit();
}

DlHcclFunction &DlHcclFunction::get_instance() {
  static DlHcclFunction dlHcclFunction;
  return dlHcclFunction;
}

HcclResult DlHcclFunction::init() {
  if (dl_hccl_handle != nullptr && dl_hcomm_handle != nullptr) {
    return HCCL_SUCCESS;
  }

  std::lock_guard<std::mutex> lock(handleMutex_);

  if (dl_hccl_handle == nullptr) {
    dl_hccl_handle = dlopen("libhccl.so", RTLD_LAZY);
    CHK_PRT_RET(dl_hccl_handle == nullptr, HCCL_ERROR("load fail: libhccl.so no found"), HCCL_E_PTR);
  }

  if (dl_hcomm_handle == nullptr) {
    dl_hcomm_handle = dlopen("libhcomm.so", RTLD_LAZY);
    CHK_PRT_RET(dl_hcomm_handle == nullptr, HCCL_ERROR("load fail: libhcomm.so no found"), HCCL_E_PTR);
  }

  // libhccl.so func
  dlHcclAllGatherFunc = (HcclResult (*)(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType,
                                        HcclComm comm, aclrtStream stream))dlsym(dl_hccl_handle, "HcclAllGather");
  CHK_PTR_NULL(dlHcclAllGatherFunc);

  dlHcclAllGatherVFunc =
      (HcclResult (*)(void *sendBuf, uint64_t sendCount, void *recvBuf, const void *recvCounts, const void *recvDispls,
                      HcclDataType dataType, HcclComm comm, aclrtStream stream))dlsym(dl_hccl_handle, "HcclAllGatherV");
  CHK_PTR_NULL(dlHcclAllGatherVFunc);

  dlHcclAllReduceFunc =
      (HcclResult (*)(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
                      HcclComm comm, aclrtStream stream))dlsym(dl_hccl_handle, "HcclAllReduce");
  CHK_PTR_NULL(dlHcclAllReduceFunc);

  dlHcclBroadcastFunc = (HcclResult (*)(void *buf, uint64_t count, HcclDataType dataType, uint32_t root, HcclComm comm,
                                        aclrtStream stream))dlsym(dl_hccl_handle, "HcclBroadcast");
  CHK_PTR_NULL(dlHcclBroadcastFunc);

  dlHcclReduceScatterFunc =
      (HcclResult (*)(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType, HcclReduceOp op,
                      HcclComm comm, aclrtStream stream))dlsym(dl_hccl_handle, "HcclReduceScatter");
  CHK_PTR_NULL(dlHcclReduceScatterFunc);

  dlHcclReduceScatterVFunc =
      (HcclResult (*)(void *sendBuf, const void *sendCounts, const void *sendDispls, void *recvBuf, uint64_t recvCount,
                      HcclDataType dataType, HcclReduceOp op, HcclComm comm,
                      aclrtStream stream))dlsym(dl_hccl_handle, "HcclReduceScatterV");
  CHK_PTR_NULL(dlHcclReduceScatterVFunc);

  dlHcclAlltoAllVCFunc =
      (HcclResult (*)(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType, const void *recvBuf,
                      HcclDataType recvType, HcclComm comm, aclrtStream stream))dlsym(dl_hccl_handle, "HcclAlltoAllVC");
  CHK_PTR_NULL(dlHcclAlltoAllVCFunc);

  dlHcclAlltoAllVFunc =
      (HcclResult (*)(const void *sendBuf, const void *sendCounts, const void *sdispls, HcclDataType sendType,
                      const void *recvBuf, const void *recvCounts, const void *rdispls, HcclDataType recvType,
                      HcclComm comm, aclrtStream stream))dlsym(dl_hccl_handle, "HcclAlltoAllV");
  CHK_PTR_NULL(dlHcclAlltoAllVFunc);

  dlHcclAlltoAllFunc = (HcclResult (*)(const void *sendBuf, uint64_t sendCount, HcclDataType sendType,
                                       const void *recvBuf, uint64_t recvCount, HcclDataType recvType, HcclComm comm,
                                       aclrtStream stream))dlsym(dl_hccl_handle, "HcclAlltoAll");
  CHK_PTR_NULL(dlHcclAlltoAllFunc);

  dlHcclReduceFunc =
      (HcclResult (*)(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
                      uint32_t root, HcclComm comm, aclrtStream stream))dlsym(dl_hccl_handle, "HcclReduce");
  CHK_PTR_NULL(dlHcclReduceFunc);

  dlHcclSendFunc = (HcclResult (*)(void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t destRank,
                                   HcclComm comm, aclrtStream stream))dlsym(dl_hccl_handle, "HcclSend");
  CHK_PTR_NULL(dlHcclSendFunc);

  dlHcclRecvFunc = (HcclResult (*)(void *recvBuf, uint64_t count, HcclDataType dataType, uint32_t srcRank,
                                   HcclComm comm, aclrtStream stream))dlsym(dl_hccl_handle, "HcclRecv");
  CHK_PTR_NULL(dlHcclRecvFunc);

  // libhcomm.so func
  dlHcomGetandClearOverFlowTasksFunc = (HcclResult (*)(const char *group, hccl::HcclDumpInfo **hcclDumpInfoPtr,
                                                       s32 *len))dlsym(dl_hcomm_handle, "HcomGetandClearOverFlowTasks");
  CHK_PTR_NULL(dlHcomGetandClearOverFlowTasksFunc);
  
  auto ret = initHcclGraphModeFunctions();
  if(ret != HCCL_SUCCESS) {
    isHcclGraphModeFunctionsLoaded_ = false;
    HCCL_WARNING("[DlHcclFunction]load hccl graph mode functions fail\n");
  } else {
    isHcclGraphModeFunctionsLoaded_ = true;
  }
  return HCCL_SUCCESS;
}

HcclResult DlHcclFunction::initHcclGraphModeFunctions() {
  // 图模式相关函数
  dlHcclCreateOpParamGraphModeFunc = (HcclResult (*)(OpParamGraphModePtr *opParam))dlsym(dl_hccl_handle, "HcclCreateOpParamGraphMode");
  CHK_PRT_RET(dlHcclCreateOpParamGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclCreateOpParamGraphMode function fail."), HCCL_E_PTR);

  dlHcclDestroyOpParamGraphModeFunc = (HcclResult (*)(OpParamGraphModePtr opParam))dlsym(dl_hccl_handle, "HcclDestroyOpParamGraphMode");
  CHK_PRT_RET(dlHcclDestroyOpParamGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclDestroyOpParamGraphMode function fail."), HCCL_E_PTR);

  dlHcclSetOpParamGraphModeOpTypeFunc = (HcclResult (*)(OpParamGraphModePtr opParam, const char *opType))dlsym(dl_hccl_handle, "HcclSetOpParamGraphModeOpType");
  CHK_PRT_RET(dlHcclSetOpParamGraphModeOpTypeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclSetOpParamGraphModeOpType function fail."), HCCL_E_PTR);

  dlHcclSetOpParamGraphModeDataCountFunc = (HcclResult (*)(OpParamGraphModePtr opParam, const u64 *count))dlsym(dl_hccl_handle, "HcclSetOpParamGraphModeDataCount");
  CHK_PRT_RET(dlHcclSetOpParamGraphModeDataCountFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclSetOpParamGraphModeDataCount function fail."), HCCL_E_PTR);

  dlHcclSetOpParamGraphModeRankSizeFunc = (HcclResult (*)(OpParamGraphModePtr opParam, const u32 *rankSize))dlsym(dl_hccl_handle, "HcclSetOpParamGraphModeRankSize");
  CHK_PRT_RET(dlHcclSetOpParamGraphModeRankSizeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclSetOpParamGraphModeRankSize function fail."), HCCL_E_PTR);

  dlHcclSetOpParamGraphModeHCCLBufferSizeFunc = (HcclResult (*)(OpParamGraphModePtr opParam, const u64 *cclBufferSize))dlsym(dl_hccl_handle, "HcclSetOpParamGraphModeHCCLBufferSize");
  CHK_PRT_RET(dlHcclSetOpParamGraphModeHCCLBufferSizeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclSetOpParamGraphModeHCCLBufferSize function fail."), HCCL_E_PTR);

  dlHcclSetOpParamGraphModeDataTypeFunc = (HcclResult (*)(OpParamGraphModePtr opParam, const HcclDataType dataType))dlsym(dl_hccl_handle, "HcclSetOpParamGraphModeDataType");
  CHK_PRT_RET(dlHcclSetOpParamGraphModeDataTypeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclSetOpParamGraphModeDataType function fail."), HCCL_E_PTR);

  dlHcclSetAivSelectOpParamGraphModeFunc = (HcclResult (*)(OpParamGraphModePtr opParam, u32 aivCoreLimit))dlsym(dl_hccl_handle, "HcclSetAivSelectOpParamGraphMode");
  CHK_PRT_RET(dlHcclSetAivSelectOpParamGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclSetAivSelectOpParamGraphMode function fail."), HCCL_E_PTR);

  dlHcclCalcOpResOfflineGraphModeFunc = (HcclResult (*)(OpParamGraphModePtr opParam, u64 *opMemSize, 
      u32 *streamNum, u32 *taskNum, u32 *aivCoreNum))dlsym(dl_hccl_handle, "HcclCalcOpResOfflineGraphMode");
  CHK_PRT_RET(dlHcclCalcOpResOfflineGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclCalcOpResOfflineGraphMode function fail."), HCCL_E_PTR);

  dlHcclCalcOpResOnlineGraphModeFunc = (HcclResult (*)(OpParamGraphModePtr opParam, u64 *opMemSize, 
      u32 *streamNum, u32 *taskNum, u32 *aivCoreNum))dlsym(dl_hccl_handle, "HcclCalcOpResOnlineGraphMode");
  CHK_PRT_RET(dlHcclCalcOpResOnlineGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclCalcOpResOnlineGraphMode function fail."), HCCL_E_PTR);

  dlHcclAllGatherGraphModeFunc = (HcclResult (*)(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, 
      const char *group, void *stream, const char *tag, void **streams, size_t streamCount, 
      void *scratchMemAddr, uint64_t scratchMemSize))dlsym(dl_hccl_handle, "HcclAllGatherGraphMode");
  CHK_PRT_RET(dlHcclAllGatherGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclAllGatherGraphMode function fail."), HCCL_E_PTR);
  
  dlHcclBroadcastGraphModeFunc = (HcclResult (*)(void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t root, 
      const char *group, void *stream, const char *tag, void **streams, size_t streamCount, 
      void *scratchMemAddr, uint64_t scratchMemSize))dlsym(dl_hccl_handle, "HcclBroadcastGraphMode");
  CHK_PRT_RET(dlHcclBroadcastGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclBroadcastGraphMode function fail."), HCCL_E_PTR);

  dlHcclReduceScatterVGraphModeFunc = (HcclResult (*)(void *sendBuf, const void *sendCounts, const void *sendDispls, 
      void *recvBuf, uint64_t recvCount, HcclDataType dataType, HcclReduceOp reduceOp, 
      const char *group, void *stream, const char *tag, void **streams, size_t streamCount, 
      void *scratchMemAddr, uint64_t scratchMemSize))dlsym(dl_hccl_handle, "HcclReduceScatterVGraphMode");
  CHK_PRT_RET(dlHcclReduceScatterVGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclReduceScatterVGraphMode function fail."), HCCL_E_PTR);

  dlHcclAllGatherVGraphModeFunc = (HcclResult (*)(void *sendBuf, void *recvBuf, uint64_t sendCount, 
      const void *recvCounts, const void *recvDispls, HcclDataType dataType, 
      const char *group, void *stream, const char *tag, void **streams, size_t streamCount, 
      void *scratchMemAddr, uint64_t scratchMemSize))dlsym(dl_hccl_handle, "HcclAllGatherVGraphMode");
  CHK_PRT_RET(dlHcclAllGatherVGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclAllGatherVGraphMode function fail."), HCCL_E_PTR);

  dlHcclAlltoAllGraphModeFunc = (HcclResult (*)(const void *sendBuf, uint64_t sendCount, HcclDataType sendType, 
      const void *recvBuf, uint64_t recvCount, HcclDataType recvType, 
      const char *group, void *stream, const char *tag, void **streams, size_t streamCount, 
      void *scratchMemAddr, uint64_t scratchMemSize))dlsym(dl_hccl_handle, "HcclAlltoAllGraphMode");
  CHK_PRT_RET(dlHcclAlltoAllGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclAlltoAllGraphMode function fail."), HCCL_E_PTR);

  dlHcclAlltoAllVGraphModeFunc = (HcclResult (*)(const void *sendBuf, const void *sendCounts, const void *sendDispls, 
      HcclDataType sendType, const void *recvBuf, const void *recvCounts, const void *recvDispls, 
      HcclDataType recvType, const char *group, void *stream, const char *tag, void **streams, 
      size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize))dlsym(dl_hccl_handle, "HcclAlltoAllVGraphMode");
  CHK_PRT_RET(dlHcclAlltoAllVGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclAlltoAllVGraphMode function fail."), HCCL_E_PTR);

  dlHcclAlltoAllVCGraphModeFunc = (HcclResult (*)(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType, 
      const void *recvBuf, HcclDataType recvType, const char *group, void *stream, 
      const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, 
      uint64_t scratchMemSize))dlsym(dl_hccl_handle, "HcclAlltoAllVCGraphMode");
  CHK_PRT_RET(dlHcclAlltoAllVCGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclAlltoAllVCGraphMode function fail."), HCCL_E_PTR);

  dlHcclSendGraphModeFunc = (HcclResult (*)(void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t destRank, 
      const char *group, void *stream, const char *tag, void **streams, size_t streamCount, 
      void *scratchMemAddr, uint64_t scratchMemSize))dlsym(dl_hccl_handle, "HcclSendGraphMode");
  CHK_PRT_RET(dlHcclSendGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclSendGraphMode function fail."), HCCL_E_PTR);

  dlHcclRecvGraphModeFunc = (HcclResult (*)(void *recvBuf, uint64_t count, HcclDataType dataType, uint32_t srcRank, 
      const char *group, void *stream, const char *tag, void **streams, size_t streamCount, 
      void *scratchMemAddr, uint64_t scratchMemSize))dlsym(dl_hccl_handle, "HcclRecvGraphMode");
  CHK_PRT_RET(dlHcclRecvGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclRecvGraphMode function fail."), HCCL_E_PTR);

  dlHcclAllReduceGraphModeFunc = (HcclResult (*)(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, 
      HcclReduceOp reduceOp, const char *group, void *stream, const char *tag, 
      void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize))dlsym(dl_hccl_handle, "HcclAllReduceGraphMode");
  CHK_PRT_RET(dlHcclAllReduceGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclAllReduceGraphMode function fail."), HCCL_E_PTR);

  dlHcclReduceGraphModeFunc = (HcclResult (*)(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, 
      HcclReduceOp reduceOp, uint32_t root, const char *group, void *stream, 
      const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, 
      uint64_t scratchMemSize))dlsym(dl_hccl_handle, "HcclReduceGraphMode");
  CHK_PRT_RET(dlHcclReduceGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclReduceGraphMode function fail."), HCCL_E_PTR);
  
  dlHcclReduceScatterGraphModeFunc = (HcclResult (*)(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, 
      HcclReduceOp reduceOp, const char *group, void *stream, const char *tag, 
      void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize))dlsym(dl_hccl_handle, "HcclReduceScatterGraphMode");
  CHK_PRT_RET(dlHcclReduceScatterGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclReduceScatterGraphMode function fail."), HCCL_E_PTR);

  dlHcclSetAivCoreLimitGraphModeFunc = (HcclResult (*)(const char *group, u32 aivCoreLimit))dlsym(dl_hccl_handle, "HcclSetAivCoreLimitGraphMode");
  CHK_PRT_RET(dlHcclSetAivCoreLimitGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclSetAivCoreLimitGraphMode function fail."), HCCL_E_PTR);

  dlHcclSelectAlgGraphModeFunc = (HcclResult (*)(const char *group, u64 count, HcclDataType dataType, HcclReduceOp op, HcclCMDType opType,
      int32_t aivCoreLimit, bool *ifAiv, char *algName))dlsym(dl_hccl_handle, "HcclSelectAlgGraphMode");
  CHK_PRT_RET(dlHcclSelectAlgGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclSelectAlgGraphMode function fail."), HCCL_E_PTR);

  dlHcclCalcAivCoreNumGraphModeFunc = (HcclResult (*)(u32 aivCoreLimit, u32 *blockDim))dlsym(dl_hccl_handle, "HcclCalcAivCoreNumGraphMode");
  CHK_PRT_RET(dlHcclCalcAivCoreNumGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclCalcAivCoreNumGraphMode function fail."), HCCL_E_PTR);

  dlHcclGetAlgExecParamGraphModeFunc = (HcclResult (*)(const char *tag, const char *group, u64 count, void *inputPtr, void *outputPtr,
      HcclCMDType opType, bool clearEnable, HcclDataType dataType, HcclReduceOp op,
      void **commContext, u64 *len, u32 aivCoreLimit))dlsym(dl_hccl_handle, "HcclGetAlgExecParamGraphMode");
  CHK_PRT_RET(dlHcclGetAlgExecParamGraphModeFunc == nullptr,
      HCCL_RUN_WARNING("[DlHcclFunction]load HcclGetAlgExecParamGraphMode function fail."), HCCL_E_PTR);

  return HCCL_SUCCESS;
}

bool DlHcclFunction::isLoadHcclGraphModeFunctions() const {
  return isHcclGraphModeFunctionsLoaded_;
};

void DlHcclFunction::deinit() {
  if (dl_hccl_handle != nullptr) {
    dlclose(dl_hccl_handle);
    dl_hccl_handle = nullptr;
  }

  if (dl_hcomm_handle != nullptr) {
    dlclose(dl_hcomm_handle);
    dl_hcomm_handle = nullptr;
  }
}

HcclResult DlHcclFunction::dlHcclAllReduce(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType,
                                           HcclReduceOp op, HcclComm comm, aclrtStream stream) {
  return dlHcclAllReduceFunc(sendBuf, recvBuf, count, dataType, op, comm, stream);
}

HcclResult DlHcclFunction::dlHcclBroadcast(void *buf, uint64_t count, HcclDataType dataType, uint32_t root,
                                           HcclComm comm, aclrtStream stream) {
  return dlHcclBroadcastFunc(buf, count, dataType, root, comm, stream);
}

HcclResult DlHcclFunction::dlHcclReduceScatter(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType,
                                               HcclReduceOp op, HcclComm comm, aclrtStream stream) {
  return dlHcclReduceScatterFunc(sendBuf, recvBuf, recvCount, dataType, op, comm, stream);
}

HcclResult DlHcclFunction::dlHcclReduceScatterV(void *sendBuf, const void *sendCounts, const void *sendDispls,
                                                void *recvBuf, uint64_t recvCount, HcclDataType dataType,
                                                HcclReduceOp op, HcclComm comm, aclrtStream stream) {
  return dlHcclReduceScatterVFunc(sendBuf, sendCounts, sendDispls, recvBuf, recvCount, dataType, op, comm, stream);
}

HcclResult DlHcclFunction::dlHcclAllGather(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType,
                                           HcclComm comm, aclrtStream stream) {
  return dlHcclAllGatherFunc(sendBuf, recvBuf, sendCount, dataType, comm, stream);
}

HcclResult DlHcclFunction::dlHcclAllGatherV(void *sendBuf, uint64_t sendCount, void *recvBuf, const void *recvCounts,
                                            const void *recvDispls, HcclDataType dataType, HcclComm comm,
                                            aclrtStream stream) {
  return dlHcclAllGatherVFunc(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, dataType, comm, stream);
}

HcclResult DlHcclFunction::dlHcclSend(void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t destRank,
                                      HcclComm comm, aclrtStream stream) {
  return dlHcclSendFunc(sendBuf, count, dataType, destRank, comm, stream);
}

HcclResult DlHcclFunction::dlHcclRecv(void *recvBuf, uint64_t count, HcclDataType dataType, uint32_t srcRank,
                                      HcclComm comm, aclrtStream stream) {
  return dlHcclRecvFunc(recvBuf, count, dataType, srcRank, comm, stream);
}

HcclResult DlHcclFunction::dlHcclAlltoAllVC(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType,
                                            const void *recvBuf, HcclDataType recvType, HcclComm comm,
                                            aclrtStream stream) {
  return dlHcclAlltoAllVCFunc(sendBuf, sendCountMatrix, sendType, recvBuf, recvType, comm, stream);
}

HcclResult DlHcclFunction::dlHcclAlltoAllV(const void *sendBuf, const void *sendCounts, const void *sdispls,
                                           HcclDataType sendType, const void *recvBuf, const void *recvCounts,
                                           const void *rdispls, HcclDataType recvType, HcclComm comm,
                                           aclrtStream stream) {
  return dlHcclAlltoAllVFunc(sendBuf, sendCounts, sdispls, sendType, recvBuf, recvCounts, rdispls, recvType, comm,
                             stream);
}

HcclResult DlHcclFunction::dlHcclAlltoAll(const void *sendBuf, uint64_t sendCount, HcclDataType sendType,
                                          const void *recvBuf, uint64_t recvCount, HcclDataType recvType, HcclComm comm,
                                          aclrtStream stream) {
  return dlHcclAlltoAllFunc(sendBuf, sendCount, sendType, recvBuf, recvCount, recvType, comm, stream);
}

HcclResult DlHcclFunction::dlHcclReduce(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType,
                                        HcclReduceOp op, uint32_t root, HcclComm comm, aclrtStream stream) {
  return dlHcclReduceFunc(sendBuf, recvBuf, count, dataType, op, root, comm, stream);
}

HcclResult DlHcclFunction::dlHcomGetandClearOverFlowTasks(const char *group, hccl::HcclDumpInfo **hcclDumpInfoPtr,
                                                          s32 *len) {
  return dlHcomGetandClearOverFlowTasksFunc(group, hcclDumpInfoPtr, len);
}

// 图模式相关函数实现
HcclResult DlHcclFunction::dlHcclCreateOpParamGraphMode(const OpParamGraphModePtr *opParam) {
  return dlHcclCreateOpParamGraphModeFunc(opParam);
}

HcclResult DlHcclFunction::dlHcclDestroyOpParamGraphMode(const OpParamGraphModePtr opParam) {
  return dlHcclDestroyOpParamGraphModeFunc(opParam);
}

HcclResult DlHcclFunction::dlHcclSetOpParamGraphModeOpType(OpParamGraphModePtr opParam, const char *opType) {
  return dlHcclSetOpParamGraphModeOpTypeFunc(opParam, opType);
}

HcclResult DlHcclFunction::dlHcclSetOpParamGraphModeDataCount(OpParamGraphModePtr opParam, const u64 *dataCount) {
  return dlHcclSetOpParamGraphModeDataCountFunc(opParam, dataCount);
}

HcclResult DlHcclFunction::dlHcclSetOpParamGraphModeRankSize(OpParamGraphModePtr opParam, const u32 *rankSize) {
  return dlHcclSetOpParamGraphModeRankSizeFunc(opParam, rankSize);
}

HcclResult DlHcclFunction::dlHcclSetOpParamGraphModeHCCLBufferSize(OpParamGraphModePtr opParam, const u64 *hcclBufferSize) {
  return dlHcclSetOpParamGraphModeHCCLBufferSizeFunc(opParam, hcclBufferSize);
}

HcclResult DlHcclFunction::dlHcclSetOpParamGraphModeDataType(OpParamGraphModePtr opParam, const HcclDataType dataType) {
  return dlHcclSetOpParamGraphModeDataTypeFunc(opParam, dataType);
}

HcclResult DlHcclFunction::dlHcclSetAivSelectOpParamGraphMode(OpParamGraphModePtr opParam, u32 aivCoreLimit) {
  return dlHcclSetAivSelectOpParamGraphModeFunc(opParam, aivCoreLimit);
}

HcclResult DlHcclFunction::dlHcclCalcOpResOfflineGraphMode(OpParamGraphModePtr opParam, u64 *opMemSize, u32 *streamNum, u32 *taskNum, const u32 *aivCoreNum) {
  return dlHcclCalcOpResOfflineGraphModeFunc(opParam, opMemSize, streamNum, taskNum, aivCoreNum);
}

HcclResult DlHcclFunction::dlHcclCalcOpResOnlineGraphMode(OpParamGraphModePtr opParam, u64 *opMemSize, u32 *streamNum, u32 *taskNum, const u32 *aivCoreNum) {
  return dlHcclCalcOpResOnlineGraphModeFunc(opParam, opMemSize, streamNum, taskNum, aivCoreNum);
}

HcclResult DlHcclFunction::dlHcclAllGatherGraphMode(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType, const char *group, void *stream, const char *optag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    return dlHcclAllGatherGraphModeFunc(sendBuf, recvBuf, sendCount, dataType, group, stream, optag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult DlHcclFunction::dlHcclBroadcastGraphMode(void *buf, uint64_t count, HcclDataType dataType, uint32_t root, const char *group, void *stream, const char *optag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    return dlHcclBroadcastGraphModeFunc(buf, count, dataType, root, group, stream, optag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult DlHcclFunction::dlHcclReduceScatterVGraphMode(void *sendBuf, const void *sendCounts, const void *sendDispls, void *recvBuf,
                                   uint64_t recvCount, HcclDataType dataType, HcclReduceOp op, const char *group, void *stream, const char *optag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
  return dlHcclReduceScatterVGraphModeFunc(sendBuf, sendCounts, sendDispls, recvBuf, recvCount, dataType, op, group, stream, optag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult DlHcclFunction::dlHcclAllGatherVGraphMode(void *sendBuf, void *recvBuf, uint64_t sendCount, const void *recvCounts, const void *recvDispls, HcclDataType dataType, const char *group, void *stream, const char *optag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
  return dlHcclAllGatherVGraphModeFunc(sendBuf, recvBuf, sendCount, recvCounts, recvDispls, dataType, group, stream, optag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult DlHcclFunction::dlHcclAlltoAllGraphMode(const void *sendBuf, uint64_t sendCount, HcclDataType sendType, const void *recvBuf, uint64_t recvCount, HcclDataType recvType,
  const char *group, void *stream, const char *optag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    return dlHcclAlltoAllGraphModeFunc(sendBuf, sendCount, sendType, recvBuf, recvCount, recvType,
      group, stream, optag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult DlHcclFunction::dlHcclAlltoAllVGraphMode(const void *sendBuf, const void *sendCounts, const void *sdispls, HcclDataType sendType,
  const void *recvBuf, const void *recvCounts, const void *rdispls, HcclDataType recvType,
  const char *group, void *stream, const char *optag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    return dlHcclAlltoAllVGraphModeFunc(sendBuf, sendCounts, sdispls, sendType, recvBuf, recvCounts, rdispls, recvType,
      group, stream, optag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult DlHcclFunction::dlHcclAlltoAllVCGraphMode(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType, const void *recvBuf, HcclDataType recvType,
  const char *group, void *stream, const char *optag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    return dlHcclAlltoAllVCGraphModeFunc(sendBuf, sendCountMatrix, sendType, recvBuf, recvType,
      group, stream, optag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult DlHcclFunction::dlHcclSendGraphMode(void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t destRank, const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    return dlHcclSendGraphModeFunc(sendBuf, count, dataType, destRank, group, stream, tag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult DlHcclFunction::dlHcclRecvGraphMode(void *recvBuf, uint64_t count, HcclDataType dataType, uint32_t srcRank, const char *group, void *stream, const char *tag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    return dlHcclRecvGraphModeFunc(recvBuf, count, dataType, srcRank, group, stream, tag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult DlHcclFunction::dlHcclAllReduceGraphMode(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType, HcclReduceOp op, const char *group, void *stream, const char *optag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    return dlHcclAllReduceGraphModeFunc(sendBuf, recvBuf, sendCount, dataType, op, group, stream, optag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult DlHcclFunction::dlHcclReduceGraphMode(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op, uint32_t root,
                                                 const char *group, void *stream, const char *optag, void **streams, size_t streamCount,
                                                 void *scratchMemAddr, uint64_t scratchMemSize) {
  return dlHcclReduceGraphModeFunc(sendBuf, recvBuf, count, dataType, op, root, group, stream, optag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult DlHcclFunction::dlHcclReduceScatterGraphMode(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType, HcclReduceOp op, const char *group, void *stream, const char *optag, void **streams, size_t streamCount, void *scratchMemAddr, uint64_t scratchMemSize) {
    return dlHcclReduceScatterGraphModeFunc(sendBuf, recvBuf, recvCount, dataType, op, group, stream, optag, streams, streamCount, scratchMemAddr, scratchMemSize);
}

HcclResult DlHcclFunction::dlHcclSetAivCoreLimitGraphMode(const char *group, u32 aivCoreLimit) {
    return dlHcclSetAivCoreLimitGraphModeFunc(group, aivCoreLimit);
}

HcclResult DlHcclFunction::dlHcclSelectAlgGraphMode(const char *group, u64 count, HcclDataType dataType, HcclReduceOp op, HcclCMDType opType,
                           int32_t aivCoreLimit, bool *ifAiv, char *algName) {
    return dlHcclSelectAlgGraphModeFunc(group, count, dataType, op, opType, aivCoreLimit, ifAiv, algName);
}

HcclResult DlHcclFunction::dlHcclCalcAivCoreNumGraphMode(u32 aivCoreLimit, u32 *blockDim) {
    return dlHcclCalcAivCoreNumGraphModeFunc(aivCoreLimit, blockDim);
}

HcclResult DlHcclFunction::dlHcclGetAlgExecParamGraphMode(const char *tag, const char *group, u64 count, void *inputPtr, void *outputPtr,
                                 HcclCMDType opType, bool clearEnable, HcclDataType dataType, HcclReduceOp op,
                                 void **commContext, u64 *len, u32 aivCoreLimit) {
    return dlHcclGetAlgExecParamGraphModeFunc(tag, group, count, inputPtr, outputPtr, opType, clearEnable, dataType, op, commContext, len, aivCoreLimit);
}