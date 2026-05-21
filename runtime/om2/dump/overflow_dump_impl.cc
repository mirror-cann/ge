/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/runtime/dump/overflow_dump_impl.h"
#include "framework/runtime/dump/dump_config.h"
#include "framework/common/debug/ge_log.h"
#include "runtime/rt.h"
#include "acl/acl_rt.h"

namespace ge {
namespace dump {
namespace {
constexpr size_t kOpDebugMemorySize = 2048UL;
constexpr size_t kDebugP2pSize = 8UL;
}  // namespace

OverflowDumpImpl::OverflowDumpImpl() = default;

OverflowDumpImpl::~OverflowDumpImpl() {
  Clear();
}

Status OverflowDumpImpl::RegisterForModel(rtModel_t rt_model_handle) {
  GELOGD("OverflowDumpImpl::RegisterForModel, rt_model_handle=%p", rt_model_handle);
  if (rt_model_handle == nullptr) {
    GELOGE(PARAM_INVALID, "rt_model_handle is null");
    return PARAM_INVALID;
  }

  // 1. 分配 Overflow 内存
  Status ret = MallocMemForOpdebug();
  if (ret != SUCCESS) {
    GELOGE(ret, "Malloc mem for opdebug failed");
    return ret;
  }

  // 2. 调用 RT 接口注册 Overflow
  const uint32_t op_debug_mode = DumpConfig::Instance().GetOpDebugMode();
  GELOGD("Overflow register: op_debug_mode=%u", op_debug_mode);
  uint32_t debug_stream_id = 0U;
  uint32_t debug_task_id = 0U;
  rtError_t rt_ret = rtDebugRegister(rt_model_handle, op_debug_mode, op_debug_addr_,
                                      &debug_stream_id, &debug_task_id);
  if (rt_ret != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "rtDebugRegister error, ret: 0x%X", rt_ret);
    Clear();  // 注册失败，释放已分配的内存
    return RT_FAILED;
  }

  GELOGD("Overflow register success, debug_task_id=%u, debug_stream_id=%u",
         debug_task_id, debug_stream_id);

  // 3. 保存 debug id
  op_debug_task_id_ = debug_task_id;
  op_debug_stream_id_ = debug_stream_id;
  is_op_debug_enabled_ = true;

  return SUCCESS;
}

void OverflowDumpImpl::UnregisterForModel(rtModel_t rt_model_handle) {
  GELOGD("OverflowDumpImpl::UnregisterForModel, rt_model_handle=%p, is_registered=%u",
         rt_model_handle, static_cast<uint32_t>(is_op_debug_enabled_));
  if (!is_op_debug_enabled_) {
    GELOGD("Overflow not registered, skip unregister");
    return;
  }
  if (rt_model_handle != nullptr) {
    rtError_t rt_ret = rtDebugUnRegister(rt_model_handle);
    if (rt_ret != RT_ERROR_NONE) {
      GELOGW("rtDebugUnRegister failed, ret: 0x%X", rt_ret);
    }
  }
  Clear();
}

bool OverflowDumpImpl::IsOpDebugEnabled() const {
  return is_op_debug_enabled_;
}

uint32_t OverflowDumpImpl::GetOpDebugTaskId() const {
  return op_debug_task_id_;
}

uint32_t OverflowDumpImpl::GetOpDebugStreamId() const {
  return op_debug_stream_id_;
}

void* OverflowDumpImpl::GetOpDebugAddr() const {
  return p2p_debug_addr_;
}

void OverflowDumpImpl::Clear() {
  // 释放 Overflow 内存
  if (op_debug_addr_ != nullptr) {
    aclError acl_ret = aclrtFree(op_debug_addr_);
    if (acl_ret != ACL_SUCCESS) {
      GELOGW("aclrtFree op_debug_addr_ failed, ret: 0x%X", acl_ret);
    }
    op_debug_addr_ = nullptr;
  }
  if (p2p_debug_addr_ != nullptr) {
    aclError acl_ret = aclrtFree(p2p_debug_addr_);
    if (acl_ret != ACL_SUCCESS) {
      GELOGW("aclrtFree p2p_debug_addr_ failed, ret: 0x%X", acl_ret);
    }
    p2p_debug_addr_ = nullptr;
  }
  is_op_debug_enabled_ = false;
  op_debug_task_id_ = 0U;
  op_debug_stream_id_ = 0U;
}

Status OverflowDumpImpl::MallocMemForOpdebug() {
  // 1. 分配 OpDebug 内存
  aclError rt_ret = aclrtMalloc(&op_debug_addr_, kOpDebugMemorySize, ACL_MEM_MALLOC_HUGE_FIRST);
  if (rt_ret != ACL_SUCCESS) {
    GELOGE(RT_FAILED, "Call aclrtMalloc for op_debug failed, ret:%d", rt_ret);
    return RT_FAILED;
  }

  // 2. 分配 P2P 内存，保存 OpDebug 地址的指针
  const uint64_t debug_addrs_tmp = reinterpret_cast<uint64_t>(op_debug_addr_);
  rt_ret = aclrtMalloc(&p2p_debug_addr_, kDebugP2pSize, ACL_MEM_TYPE_HIGH_BAND_WIDTH);
  if (rt_ret != ACL_SUCCESS) {
    GELOGE(RT_FAILED, "Call aclrtMalloc for p2p failed, ret:%d", rt_ret);
    (void)aclrtFree(op_debug_addr_);
    op_debug_addr_ = nullptr;
    return RT_FAILED;
  }

  // 3. 将 OpDebug 地址拷贝到 P2P 内存（H2D）
  rt_ret = aclrtMemcpy(p2p_debug_addr_, sizeof(uint64_t), &debug_addrs_tmp,
                        sizeof(uint64_t), ACL_MEMCPY_HOST_TO_DEVICE);
  if (rt_ret != ACL_SUCCESS) {
    GELOGE(RT_FAILED, "Call aclrtMemcpy failed, ret:%d", rt_ret);
    (void)aclrtFree(p2p_debug_addr_);
    (void)aclrtFree(op_debug_addr_);
    op_debug_addr_ = nullptr;
    p2p_debug_addr_ = nullptr;
    return RT_FAILED;
  }

  return SUCCESS;
}

}  // namespace dump
}  // namespace ge
