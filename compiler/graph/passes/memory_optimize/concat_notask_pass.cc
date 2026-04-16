/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/passes/memory_optimize/concat_notask_pass.h"
#include "common/checker.h"

namespace ge {
bool ConcatNotaskPass::CheckDim(const ge::OpDescPtr &op_desc) const {
  int64_t concat_dim = 0;

  (void)ge::AttrUtils::GetInt(op_desc, "concat_dim", concat_dim);
  for (size_t i = 0UL; i < op_desc->GetAllInputsSize(); ++i) {
    ge::GeShape input_orinal_shape = op_desc->GetInputDesc(i).GetOriginShape();

    if (concat_dim < 0) {
      GELOGD("Concat_dim[%lld] is negative number, change it to positive.", concat_dim);
      concat_dim = static_cast<int64_t>(input_orinal_shape.GetDimNum()) + concat_dim;
    }
    GE_ASSERT_TRUE((concat_dim >= 0) && (static_cast<size_t>(concat_dim) < input_orinal_shape.GetDimNum()));

    if (!CheckDimForInput(op_desc, concat_dim, i)) {
      return false;
    }
  }
  return true;
}

REG_PASS_OPTION("ConcatNotaskPass").LEVELS(OoLevel::kO3);
}  // namespace ge
