/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __INC_AUTOFUSE_FRAME_AUTOFUSE_FRAME_H__
#define __INC_AUTOFUSE_FRAME_AUTOFUSE_FRAME_H__

#include <cstdint>

#include "autofuse/autofuse_frame/autofuse_frames_af.h"

namespace ge {
extern "C" {
ge::Status LoweringAndCanFuse(const ge::ComputeGraphPtr &graph);
ge::Status LoweringAndCanFuseWithCounter(const ge::ComputeGraphPtr &graph, CounterPtr counter);
}
}  // namespace ge

#endif
