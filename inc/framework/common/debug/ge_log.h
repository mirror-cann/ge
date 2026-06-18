/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INC_FRAMEWORK_COMMON_DEBUG_GE_LOG_H_
#define INC_FRAMEWORK_COMMON_DEBUG_GE_LOG_H_
#include "common/ge_common/debug/ge_log.h"
#include "common/ge_common/string_util.h"
#include "graph_metadef/common/ge_common/util.h"
#include "common/ge_common/ge_inner_error_codes.h"
#include "acl/acl_rt.h"

// If expr is not ACL_ERROR_NONE, print the log and return
#define GE_CHK_ACL_RET(expr)                                                                                           \
  do {                                                                                                                 \
    const aclError _acl_ret = (expr);                                                                                  \
    if (_acl_ret != ACL_ERROR_NONE) {                                                                                  \
      REPORT_INNER_ERR_MSG("E19999", "Call %s fail, ret: 0x%X", #expr, static_cast<uint32_t>(_acl_ret));              \
      GELOGE(ge::RT_FAILED, "Call aclrt api failed, ret: 0x%X", static_cast<uint32_t>(_acl_ret));                      \
      return RT_ERROR_TO_GE_STATUS(_acl_ret);                                                                          \
    }                                                                                                                  \
  } while (false)

#define GE_ASSERT_ACL_OK(v, ...) GE_ASSERT(((v) == ACL_ERROR_NONE), __VA_ARGS__)

#endif  // INC_FRAMEWORK_COMMON_DEBUG_GE_LOG_H_
