/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hcom_graph_mc2.h"
#include "register/hidden_inputs_func_registry.h"
#include "register/op_tiling_info.h"
#include "hcom/hcom_topo_info.h"
#include "hccl/hcom.h"
#include "hcom_op_utils.h"
#include "hcom_plugin.h"
#include "graph/debug/ge_attr_define.h"
#include "hcom_graph_optimizer.h"
#include "hcom_acl_adapter.h"

#ifndef OPEN_BUILD_PROJECT
#include "acl/acl_rt.h"
#endif
#include <dlfcn.h>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace hccl {
static constexpr u32 NEW_TILING_VERSION = 2U;
static constexpr u32 INIT_TILING_VERSION = 100U;
static constexpr u32 MAX_CC_TILING_NUM = 8U;
static constexpr u8 COMM_ENGINE_AICPU_FOR_MC2 = 2U;
static constexpr char MC2_CLIENT_SO_NAME[] = "libmc2_client.so";
static constexpr char MC2_A5_ALLOC_FUNC_NAME[] = "HcclAllocComResourceByTilingA5Mc2";
const uint32_t GROUP_NAME_OFFSET = 5;

struct Mc2InitTilingForGe {
  u32 version;
  u32 mc2HcommCnt;
  u32 offset[MAX_CC_TILING_NUM];
  u8 debugMode;
  u8 preparePosition;
  u16 queueNum;
  u16 commBlockNum;
  u8 devType;
  char reserved[17];
};

struct Mc2CcTilingForGe {
  u8 skipLocalRankCopy;
  u8 skipBufferWindowCopy;
  u8 stepSize;
  u8 version;
  char reserved[9];
  u8 commEngine;
  u8 srcDataType;
  u8 dstDataType;
};

using HcclAllocComResourceByTilingA5Mc2Func = HcclResult (*)(HcclComm, void *, void *, void **);

#ifndef OPEN_BUILD_PROJECT
struct A5AicpuGraphSyncResource {
  s32 devId = -1;
  std::string groupName;
  aclrtNotify notify = nullptr;
};

std::mutex g_a5AicpuGraphSyncMutex;
std::map<std::string, A5AicpuGraphSyncResource> g_a5AicpuGraphSyncResources;

std::string MakeA5AicpuGraphSyncKey(s32 devId, const char *groupName) {
  return std::to_string(devId) + ":" + groupName;
}

void DestroyA5AicpuGraphNotify(const A5AicpuGraphSyncResource &resource) {
  if (resource.notify == nullptr) {
    return;
  }

  const aclError aclRet = aclrtDestroyNotify(resource.notify);
  if (aclRet != ACL_SUCCESS) {
    HCCL_WARNING("GE_MC2_A5_RESOURCE destroy graph notify failed, group %s, devId[%d], notify[%p], "
                 "aclRet[%d].",
                 resource.groupName.c_str(), resource.devId, resource.notify, aclRet);
  } else {
    HCCL_INFO("GE_MC2_A5_RESOURCE release graph sync resource success, group %s, devId[%d], notify[%p].",
              resource.groupName.c_str(), resource.devId, resource.notify);
  }
}

void ReleaseA5AicpuGraphSyncResourceByKey(s32 devId, const char *groupName) {
  if (groupName == nullptr) {
    return;
  }

  A5AicpuGraphSyncResource resource;
  bool found = false;
  {
    const std::lock_guard<std::mutex> lock(g_a5AicpuGraphSyncMutex);
    auto iter = g_a5AicpuGraphSyncResources.find(MakeA5AicpuGraphSyncKey(devId, groupName));
    if (iter != g_a5AicpuGraphSyncResources.end()) {
      resource = iter->second;
      g_a5AicpuGraphSyncResources.erase(iter);
      found = true;
    }
  }

  if (found) {
    ge::HcomTopoInfo::Instance().UnsetGroupOrderedStream(devId, groupName);
    DestroyA5AicpuGraphNotify(resource);
  }
}

HcclResult GetOrCreateA5AicpuGraphNotify(s32 devId, const char *groupName, void **notifyHandle, bool &created) {
  CHK_PTR_NULL(groupName);
  CHK_PTR_NULL(notifyHandle);
  created = false;

  const std::string key = MakeA5AicpuGraphSyncKey(devId, groupName);
  {
    const std::lock_guard<std::mutex> lock(g_a5AicpuGraphSyncMutex);
    auto iter = g_a5AicpuGraphSyncResources.find(key);
    if (iter != g_a5AicpuGraphSyncResources.end()) {
      *notifyHandle = iter->second.notify;
      return HCCL_SUCCESS;
    }
  }

  aclrtNotify notify = nullptr;
  const aclError aclRet = aclrtCreateNotify(&notify, static_cast<uint32_t>(ACL_NOTIFY_DEFAULT));
  CHK_PRT_RET(aclRet != ACL_SUCCESS || notify == nullptr,
              HCCL_ERROR("GE_MC2_A5_RESOURCE failed to create graph notify, group %s, devId[%d], "
                         "aclRet[%d], notify[%p].",
                         groupName, devId, aclRet, notify),
              HCCL_E_RUNTIME);

  {
    const std::lock_guard<std::mutex> lock(g_a5AicpuGraphSyncMutex);
    auto iter = g_a5AicpuGraphSyncResources.find(key);
    if (iter != g_a5AicpuGraphSyncResources.end()) {
      *notifyHandle = iter->second.notify;
    } else {
      A5AicpuGraphSyncResource resource;
      resource.devId = devId;
      resource.groupName = groupName;
      resource.notify = notify;
      g_a5AicpuGraphSyncResources.emplace(key, resource);
      *notifyHandle = notify;
      created = true;
      notify = nullptr;
    }
  }

  if (notify != nullptr) {
    const aclError destroyRet = aclrtDestroyNotify(notify);
    HCCL_INFO("GE_MC2_A5_RESOURCE destroy duplicated graph notify, group %s, devId[%d], "
              "notify[%p], aclRet[%d].",
              groupName, devId, notify, destroyRet);
  }
  return HCCL_SUCCESS;
}
#endif

void ReleaseA5AicpuGraphSyncResource(const char *groupName) {
#ifndef OPEN_BUILD_PROJECT
  if (groupName == nullptr) {
    return;
  }

  std::vector<A5AicpuGraphSyncResource> resources;
  {
    const std::lock_guard<std::mutex> lock(g_a5AicpuGraphSyncMutex);
    for (auto iter = g_a5AicpuGraphSyncResources.begin(); iter != g_a5AicpuGraphSyncResources.end();) {
      if (iter->second.groupName == groupName) {
        resources.emplace_back(iter->second);
        iter = g_a5AicpuGraphSyncResources.erase(iter);
      } else {
        ++iter;
      }
    }
  }

  for (const auto &resource : resources) {
    ge::HcomTopoInfo::Instance().UnsetGroupOrderedStream(resource.devId, groupName);
    DestroyA5AicpuGraphNotify(resource);
  }
#else
  (void)groupName;
#endif
}

bool IsA5DeviceType(const DevType devType) {
#ifdef MACRO_DEV_TYPE_NEW
  return devType == DevType::DEV_TYPE_950;
#else
  return devType == DevType::DEV_TYPE_910_95;
#endif
}

HcclAllocComResourceByTilingA5Mc2Func GetA5Mc2AllocFunc() {
  static HcclAllocComResourceByTilingA5Mc2Func func = nullptr;
  static void *handle = nullptr;
  static bool loaded = false;
  if (loaded) {
    return func;
  }

  loaded = true;
  handle = dlopen(MC2_CLIENT_SO_NAME, RTLD_NOW | RTLD_LOCAL);
  if (handle == nullptr) {
    const char *err = dlerror();
    HCCL_ERROR("GE_MC2_A5_RESOURCE dlopen %s failed, errmsg[%s].", MC2_CLIENT_SO_NAME,
               (err == nullptr ? "unknown" : err));
    return nullptr;
  }

  dlerror();
  void *symbol = dlsym(handle, MC2_A5_ALLOC_FUNC_NAME);
  const char *err = dlerror();
  if (err != nullptr || symbol == nullptr) {
    HCCL_ERROR("GE_MC2_A5_RESOURCE dlsym %s failed, errmsg[%s].", MC2_A5_ALLOC_FUNC_NAME,
               (err == nullptr ? "unknown" : err));
    (void)dlclose(handle);
    handle = nullptr;
    return nullptr;
  }

  // Keep the library loaded for process lifetime because func points to a symbol inside it.
  func = reinterpret_cast<HcclAllocComResourceByTilingA5Mc2Func>(symbol);
  HCCL_INFO("GE_MC2_A5_RESOURCE load %s from %s success.", MC2_A5_ALLOC_FUNC_NAME, MC2_CLIENT_SO_NAME);
  return func;
}

bool ParseMc2CommEngines(const std::string &tilingData, std::vector<u8> &commEngines) {
  if (tilingData.size() < sizeof(Mc2InitTilingForGe)) {
    HCCL_WARNING("GE_MC2_A5_RESOURCE tiling size[%zu] smaller than init tiling size[%zu].", tilingData.size(),
                 sizeof(Mc2InitTilingForGe));
    return false;
  }

  const auto *initTiling = reinterpret_cast<const Mc2InitTilingForGe *>(tilingData.data());
  if (initTiling->version != INIT_TILING_VERSION || initTiling->mc2HcommCnt > MAX_CC_TILING_NUM) {
    HCCL_WARNING("GE_MC2_A5_RESOURCE invalid init tiling version[%u] or cnt[%u].", initTiling->version,
                 initTiling->mc2HcommCnt);
    return false;
  }

  for (u32 i = 0U; i < initTiling->mc2HcommCnt; ++i) {
    const u32 offset = initTiling->offset[i];
    if (offset > tilingData.size() || tilingData.size() - offset < sizeof(Mc2CcTilingForGe)) {
      HCCL_WARNING("GE_MC2_A5_RESOURCE invalid cc tiling offset[%u], index[%u], tilingSize[%zu].", offset, i,
                   tilingData.size());
      return false;
    }
    const auto *ccTiling = reinterpret_cast<const Mc2CcTilingForGe *>(tilingData.data() + offset);
    commEngines.push_back(ccTiling->commEngine);
  }
  return true;
}

std::string FormatCommEngines(const std::vector<u8> &commEngines) {
  std::ostringstream oss;
  for (size_t i = 0U; i < commEngines.size(); ++i) {
    if (i != 0U) {
      oss << ",";
    }
    oss << static_cast<u32>(commEngines[i]);
  }
  return oss.str();
}

bool NeedUseA5AicpuMc2Resource(const DevType devType, const u32 tilingVersion, const std::string &tilingData) {
  const bool isA5 = IsA5DeviceType(devType);
  std::vector<u8> commEngines;
  bool parsed = false;
  if (tilingVersion == INIT_TILING_VERSION) {
    parsed = ParseMc2CommEngines(tilingData, commEngines);
  }
  bool allAicpu = !commEngines.empty();
  for (const u8 commEngine : commEngines) {
    if (commEngine != COMM_ENGINE_AICPU_FOR_MC2) {
      allAicpu = false;
      break;
    }
  }

  const bool useNewFlow = isA5 && parsed && allAicpu;
  HCCL_INFO("GE_MC2_A5_RESOURCE dispatch devType[%d], tilingVersion[%u], ccTilingCnt[%zu], commEngines[%s], "
            "selected[%s].", devType, tilingVersion, commEngines.size(), FormatCommEngines(commEngines).c_str(),
            (useNewFlow ? "new/asc-devkit-aicpu" : "old/hcomm"));
  if (isA5 && parsed && !allAicpu) {
    HCCL_INFO("GE_MC2_A5_RESOURCE A5 CCU keep old flow, commEngines[%s].",
              FormatCommEngines(commEngines).c_str());
  }
  return useNewFlow;
}

bool HcomGetGroupsByOpDesc(const ge::OpDescPtr &opdesc, std::vector<std::string> &groups) {
  std::string group;
  for (const auto& groupName : opdesc->GetAllAttrNames()) {
    HCCL_DEBUG("Get attr Name [%s]", groupName.c_str());
    if (groupName.substr(0, GROUP_NAME_OFFSET) == "group" && ge::AttrUtils::GetStr(opdesc, groupName, group)) {
      HCCL_INFO("Get group %s:%s of op %s.", groupName.c_str(), group.c_str(), opdesc->GetName().c_str());
      groups.push_back(group);
    }
  }

  return true;
}

u32 HcomGetTilingVersionByOpDesc(const ge::OpDescPtr &opdesc, std::string &tilingData) {
  const auto tiling = opdesc->GetExtAttr<std::shared_ptr<optiling::utils::OpRunInfo>>(ge::ATTR_NAME_OP_RUN_INFO);
  if (tiling == nullptr || *tiling == nullptr) {
    HCCL_WARNING("Failed to get tiling info of op %s.", opdesc->GetName().c_str());
    return 0U;
  }

  tilingData = (*tiling)->GetAllTilingData().str();
  const size_t tilingSize = tilingData.size();
  u32 expectedSize = sizeof(u32) + sizeof(u32);
  if (tilingSize < expectedSize) {
    HCCL_WARNING("Invalid tiling size(%zu) of op %s.", tilingSize, opdesc->GetName().c_str());
    return 0U;
  }

  return *reinterpret_cast<const u32 *>(tilingData.c_str());
}

rtStream_t HcomGetStreamByOpDesc(const ge::OpDescPtr &opdesc) {
  const auto rtList = opdesc->TryGetExtAttr<std::shared_ptr<std::vector<void *>>>("_rt_resource_list", nullptr);
  if (rtList == nullptr) {
    HCCL_ERROR("Failed to get rt list of op %s.", opdesc->GetName().c_str());
    return nullptr;
  }

  if (rtList->size() != 1) {
    HCCL_ERROR("Invalid rt list size(%zu) of op %s.", rtList->size(), opdesc->GetName().c_str());
    return nullptr;
  }

  return (*rtList)[0UL];
}

void *HcomGetContext(const rtStream_t stream, const void *tilingData, const char *groupName) {
#ifndef OPEN_BUILD_PROJECT
  DevType devType = HcomGetDeviceType();
#ifdef MACRO_DEV_TYPE_NEW
  if (devType == DevType::DEV_TYPE_950) {
#else
  if (devType == DevType::DEV_TYPE_910_95) {
#endif
    return HcomGetContextV2(stream, tilingData, groupName);
  }
#endif
  HcclComm commHandle = nullptr;
  HcclResult ret = HcomGetCommHandleByGroup(groupName, &commHandle);
  CHK_PRT_RET(ret != HCCL_SUCCESS || commHandle == nullptr,
              HCCL_ERROR("[Get][CommHandle]Failed to get hcom commHandle, group[%s] errNo[0x%016llx].", groupName,
                         HCCL_ERROR_CODE(ret)),
              nullptr);

  void *context = nullptr;
  if (tilingData != nullptr) {
    ret = HcclAllocComResourceByTiling(commHandle, stream, const_cast<void *>(tilingData), &context);
  } else {
    uint64_t streamMode = 0;
    CHK_PRT_RET(hrtStreamGetMode(stream, &streamMode) != HCCL_SUCCESS, HCCL_ERROR("Failed to get stream mode."),
                nullptr);
    ret = HcomCreateComResourceByComm(commHandle, streamMode, true, &context);
  }
  CHK_PRT_RET(ret != HCCL_SUCCESS || context == nullptr,
              HCCL_ERROR("Failed to create ComResource by tiling, errNo[0x%016llx].", HCCL_ERROR_CODE(ret)), nullptr);

  ge::HcomTopoInfo::TopoInfo topoInfo;
  CHK_PRT_RET(!ge::HcomTopoInfo::Instance().TryGetGroupTopoInfo(groupName, topoInfo),
              HCCL_ERROR("Failed to get topo info for group %s.", groupName), nullptr);

  rtStream_t opstream;
  ret = HcomGetAicpuOpStreamNotify(groupName, &opstream, 1, &topoInfo.notify_handle);
  CHK_PRT_RET(ret != HCCL_SUCCESS,
              HCCL_ERROR("Failed to get Aicpu Op Stream Notify, errNo[0x%016llx].", HCCL_ERROR_CODE(ret)), nullptr);

  HCCL_INFO("Set notify %p to group %s.", topoInfo.notify_handle, groupName);
  ge::HcomTopoInfo::Instance().SetGroupTopoInfo(groupName, topoInfo);

  HCCL_INFO("Create context(%s) for group %s successfully, address %p.", (tilingData != nullptr ? "v2" : "v1"),
            groupName, context);
  return context;
}

#ifndef OPEN_BUILD_PROJECT
void *HcomGetA5AicpuContext(const rtStream_t stream, const void *tilingData, const char *groupName) {
  HcclComm com = nullptr;
  HcclResult ret = HcomGetCommHandleByGroup(groupName, &com);
  CHK_PRT_RET(ret != HCCL_SUCCESS || com == nullptr,
              HCCL_ERROR("[Get][CommHandle]Failed to get hcom commHandle, group[%s] errNo[0x%016llx].", groupName,
                         HCCL_ERROR_CODE(ret)),
              nullptr);

  HcclAllocComResourceByTilingA5Mc2Func allocFunc = GetA5Mc2AllocFunc();
  CHK_PRT_RET(allocFunc == nullptr,
              HCCL_ERROR("GE_MC2_A5_RESOURCE failed to load asc-devkit A5 MC2 resource allocator."), nullptr);

  void *context = nullptr;
  ret = allocFunc(com, stream, const_cast<void *>(tilingData), &context);
  CHK_PRT_RET(ret != HCCL_SUCCESS || context == nullptr,
              HCCL_ERROR("GE_MC2_A5_RESOURCE failed to create A5 AICPU ComResource by tiling, errNo[0x%016llx].",
                         HCCL_ERROR_CODE(ret)),
              nullptr);

  uint64_t streamMode = 0U;
  CHK_PRT_RET(hrtStreamGetMode(stream, &streamMode) != HCCL_SUCCESS,
              HCCL_ERROR("GE_MC2_A5_RESOURCE failed to get stream mode for group %s.", groupName), nullptr);

  rtStream_t opstream = nullptr;
  ret = HcomMc2AiCpuStreamAllocAndGet(groupName, streamMode, &opstream);
  CHK_PRT_RET(ret != HCCL_SUCCESS || opstream == nullptr,
              HCCL_ERROR("GE_MC2_A5_RESOURCE failed to alloc AICPU op stream for group %s, errNo[0x%016llx].",
                         groupName, HCCL_ERROR_CODE(ret)),
              nullptr);

  s32 devId = -1;
  CHK_PRT_RET(hrtGetDeviceRefresh(&devId) != HCCL_SUCCESS,
              HCCL_ERROR("GE_MC2_A5_RESOURCE failed to get device id for group %s.", groupName), nullptr);

  void *graphNotify = nullptr;
  bool graphNotifyCreated = false;
  ret = GetOrCreateA5AicpuGraphNotify(devId, groupName, &graphNotify, graphNotifyCreated);
  CHK_PRT_RET(ret != HCCL_SUCCESS || graphNotify == nullptr,
              HCCL_ERROR("GE_MC2_A5_RESOURCE failed to get graph notify for group %s, devId[%d], "
                         "errNo[0x%016llx], notify[%p].",
                         groupName, devId, HCCL_ERROR_CODE(ret), graphNotify),
              nullptr);

  uint32_t geRet = ge::HcomTopoInfo::Instance().SetGroupOrderedStream(devId, groupName, opstream);
  if (geRet != ge::GRAPH_SUCCESS) {
    if (graphNotifyCreated) {
      ReleaseA5AicpuGraphSyncResourceByKey(devId, groupName);
    }
    HCCL_ERROR("GE_MC2_A5_RESOURCE failed to set ordered stream for group %s, devId[%d], opstream[%p].",
               groupName, devId, opstream);
    return nullptr;
  }

  ge::HcomTopoInfo::TopoInfo topoInfo;
  if (!ge::HcomTopoInfo::Instance().TryGetGroupTopoInfo(groupName, topoInfo)) {
    if (graphNotifyCreated) {
      ReleaseA5AicpuGraphSyncResourceByKey(devId, groupName);
    }
    HCCL_ERROR("GE_MC2_A5_RESOURCE failed to get topo info for group %s.", groupName);
    return nullptr;
  }

  topoInfo.notify_handle = graphNotify;
  ge::HcomTopoInfo::Instance().SetGroupTopoInfo(groupName, topoInfo);

  HCCL_INFO("GE_MC2_A5_RESOURCE prepare graph aicpu stream notify success, group %s, streamMode[%llu], "
            "opstream %p, notify %p, context %p.",
            groupName, static_cast<unsigned long long>(streamMode), opstream, graphNotify, context);
  return context;
}

void *HcomGetContextV2(const rtStream_t stream, const void *tilingData, const char *groupName) {
  HcclComm com = nullptr;
  HcclResult ret = HcomGetCommHandleByGroup(groupName, &com);

  void *context = nullptr;
  if (tilingData != nullptr) {
    ret = HcclAllocComResourceByTiling(com, stream, const_cast<void *>(tilingData), &context);
  } else {
    uint64_t streamMode = 0;
    CHK_PRT_RET(hrtStreamGetMode(stream, &streamMode) != HCCL_SUCCESS, HCCL_ERROR("Failed to get stream mode."),
                nullptr);
    ret = HcomCreateComResourceByComm(com, streamMode, true, &context);
  }
  CHK_PRT_RET(ret != HCCL_SUCCESS || context == nullptr,
              HCCL_ERROR("Failed to create ComResource by tiling, errNo[0x%016llx].", HCCL_ERROR_CODE(ret)), nullptr);

  ge::HcomTopoInfo::TopoInfo topoInfo;
  CHK_PRT_RET(!ge::HcomTopoInfo::Instance().TryGetGroupTopoInfo(groupName, topoInfo),
              HCCL_ERROR("Failed to get topo info for group %s.", groupName), nullptr);

  HCCL_INFO("Set notify %p to group %s.", topoInfo.notify_handle, groupName);
  ge::HcomTopoInfo::Instance().SetGroupTopoInfo(groupName, topoInfo);

  HCCL_INFO("Create context(%s) for group %s successfully, address %p.", (tilingData != nullptr ? "v2" : "v1"),
            groupName, context);
  return context;
}
#endif

HcclResult GetCountFromOpDesc(const ge::OpDescPtr &op, const std::string &sCollectiveType, HcclDataType dataType,
                              u64 &count, u32 rankSize) {
  u64 totalSize = 0;
  u32 dataTypeSize = 0;

  CHK_RET(SalGetDataTypeSize(dataType, dataTypeSize));
  CHK_PRT_RET(dataTypeSize == 0, HCCL_ERROR("[Get][CountFromOpDesc]dataType size is zero."), HCCL_E_PARA);

  if (sCollectiveType == HCCL_KERNEL_OP_TYPE_RECEIVE) {
    return HCCL_E_PARA;
  } else {
    for (u64 i = 0; i < op->GetInputsSize(); i++) {
      u64 blockSize;
      int64_t inputSize = 0;
      inputSize = static_cast<u64>(op->GetInputDescPtr(i)->GetShape().GetShapeSize());
      inputSize = inputSize * dataTypeSize;
      if (sCollectiveType == HCCL_KERNEL_OP_TYPE_REDUCESCATTER) {
        blockSize = static_cast<u64>(inputSize / rankSize);
      } else if (sCollectiveType == HCCL_KERNEL_OP_TYPE_ALLGATHER) {
        blockSize = static_cast<u64>(inputSize);
      } else if (sCollectiveType == HCCL_KERNEL_OP_TYPE_ALLREDUCE) {
        blockSize = static_cast<u64>(inputSize);
      } else {
        blockSize = static_cast<u64>(inputSize / rankSize);
      }
      totalSize = totalSize + blockSize;
    }
  }
  count = totalSize / dataTypeSize;
  HCCL_INFO("SPK op[%s] get count[%llu] success.", sCollectiveType.c_str(), count);
  return HCCL_SUCCESS;
}

ge::graphStatus HcomCreateComResourceMC2(const ge::OpDescPtr &opdesc, std::vector<void *> &contexts) {
  HCCL_INFO("[HcomCreateComResource]Hcomm create com resource of op %s, type %s", opdesc->GetName().c_str(),
            opdesc->GetType().c_str());

  std::vector<std::string> groups{};
  if (!HcomGetGroupsByOpDesc(opdesc, groups) || groups.empty()) {
    HCCL_ERROR("Failed to get groups of op %s", opdesc->GetName().c_str());
    return ge::GRAPH_FAILED;
  }

  const rtStream_t stream = HcomGetStreamByOpDesc(opdesc);
  if (stream == nullptr) {
    HCCL_ERROR("Failed to get attached stream of op %s", opdesc->GetName().c_str());
    return ge::GRAPH_FAILED;
  }

  std::string tilingData;
  const u32 version = HcomGetTilingVersionByOpDesc(opdesc, tilingData);
  HCCL_INFO("Tiling version of op %s is %u.", opdesc->GetName().c_str(), version);
#ifndef OPEN_BUILD_PROJECT
  const DevType devType = HcomGetDeviceType();
  const bool useA5AicpuResource = NeedUseA5AicpuMc2Resource(devType, version, tilingData);
#else
  const bool useA5AicpuResource = false;
#endif

  for (const auto& group : groups) {
    HCCL_INFO("HcomCreateComResourceMC2 group is %s.", group.c_str());
    if (group.empty()) {
      HCCL_RUN_INFO("[HcomCreateComResourceMC2] group is empty, push nullptr to context.");
      contexts.push_back(nullptr);
      continue;
    }
#ifndef OPEN_BUILD_PROJECT
    void *context = useA5AicpuResource ?
        HcomGetA5AicpuContext(stream, reinterpret_cast<const void *>(tilingData.c_str()), group.c_str()) :
        HcomGetContext(stream, reinterpret_cast<const void *>(tilingData.c_str()), group.c_str());
#else
    void *context = HcomGetContext(stream, reinterpret_cast<const void *>(tilingData.c_str()), group.c_str());
#endif
    CHK_PRT_RET(context == nullptr, HCCL_ERROR("Failed to create ComResource by tiling."), ge::GRAPH_FAILED);
    contexts.push_back(context);
  }
  return ge::GRAPH_SUCCESS;
}
REG_HIDDEN_INPUTS_FUNC(ge::HiddenInputsType::HCOM, HcomCreateComResource);
ge::graphStatus HcomCreateComResource(const ge::OpDescPtr &opdesc, std::vector<void *> &contexts) {
  HCCL_INFO("[HcomCreateComResource]Hcom create com resource of op %s.", opdesc->GetName().c_str());
  ge::graphStatus gRet = ge::GRAPH_FAILED;
  std::string sCollectiveType = opdesc->GetType();

  auto iter = HCCL_OPTYPE_NAME_MAP.find(sCollectiveType);
  HcclCMDType opType = (iter != HCCL_OPTYPE_NAME_MAP.end()) ? iter->second : HcclCMDType::HCCL_CMD_INVALID;
  if (opType == HcclCMDType::HCCL_CMD_INVALID) {
    HCCL_INFO("Select HcomCreateComResourceMC2.");
    gRet = HcomCreateComResourceMC2(opdesc, contexts);
  } else {
    HCCL_ERROR("[HcomCreateComResource]HcomCreateComResource failed, opType[%d] not support MC2, sCollectiveType[%s]",
               opType, sCollectiveType.c_str());
  }
  return gRet;
}
}  // namespace hccl
