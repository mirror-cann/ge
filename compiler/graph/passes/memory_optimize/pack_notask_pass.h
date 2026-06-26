/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_PASSES_PACK_NOTASK_PASS_H
#define GE_GRAPH_PASSES_PACK_NOTASK_PASS_H
#include "graph/passes/memory_optimize/notask_pass_base.h"

namespace ge {
class PackNotaskPass : public NotaskPassBase {
 public:
  PackNotaskPass() : NotaskPassBase("can_reused_for_pack_optimize") {}

 protected:
  bool IsTargetOp(const ge::OpDescPtr &op_desc) const override {
    return op_desc->GetType() == "Pack";
  }
  bool CheckDim(const ge::OpDescPtr &op_desc) const override;
  bool CheckFormat(const ge::OpDescPtr &op_desc) const override;
  std::string GetOpLabel() const override {
    return "pack";
  }
};
}  // namespace ge
#endif
