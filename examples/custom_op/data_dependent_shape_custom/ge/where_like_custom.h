/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef EXAMPLES_CUSTOM_OP_DATA_DEPENDENT_SHAPE_CUSTOM_GE_WHERE_LIKE_CUSTOM_H_
#define EXAMPLES_CUSTOM_OP_DATA_DEPENDENT_SHAPE_CUSTOM_GE_WHERE_LIKE_CUSTOM_H_

#include "graph/operator_reg.h"

namespace ge {
// 当前算子为自定义 Where 算子，输入类型只接受 DT_BOOL 作为示例。
REG_OP(WhereLikeCustom)
    .INPUT(x, TensorType({DT_BOOL}))
    .OUTPUT(y, TensorType({DT_INT64}))
    .OP_END_FACTORY_REG(WhereLikeCustom);
}  // namespace ge

#endif  // EXAMPLES_CUSTOM_OP_DATA_DEPENDENT_SHAPE_CUSTOM_GE_WHERE_LIKE_CUSTOM_H_
