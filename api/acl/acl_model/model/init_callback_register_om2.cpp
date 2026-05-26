/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "acl/acl_rt.h"
#include "framework/runtime/dump/model_dump_manager.h"
#include "log_inner.h"


namespace acl {

aclError Om2DumpInitCallbackFunc(const char *configStr, size_t len, void *userData)
{
    (void)configStr;
    (void)len;
    (void)userData;
    ACL_LOG_INFO("start to enter Om2DumpInitCallbackFunc");
    const auto geRet = ge::dump::ModelDumpManager::GlobalInit();
    ACL_REQUIRES_CALL_GE_OK(geRet, "[Init][Om2Dump]init om2 dump failed, ge errorCode = %u", geRet);
    return ACL_SUCCESS;
}
__attribute__((constructor)) aclError RegOm2DumpInitCallback()
{
    return aclInitCallbackRegister(ACL_REG_TYPE_OTHER, Om2DumpInitCallbackFunc, nullptr);
}
__attribute__((destructor)) aclError UnRegOm2DumpInitCallback()
{
    return aclInitCallbackUnRegister(ACL_REG_TYPE_OTHER, Om2DumpInitCallbackFunc);
}

}
