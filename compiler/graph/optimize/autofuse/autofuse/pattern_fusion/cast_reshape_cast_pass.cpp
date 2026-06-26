/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "cast_reshape_cast_pass.h"
#include "common/checker.h"
#include "graph_metadef/graph/debug/ge_util.h"
#include "graph/type/tensor_type_impl.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/node_utils.h"
#include "graph/utils/op_desc_utils.h"
#include "register/shape_inference.h"
#include "utils/auto_fuse_config.h"
#include "lowering/asc_lowerer/loop_common.h"
#include "base/err_msg.h"
#include "debug/ge_attr_define.h"
#include "lowering/lowerings.h"
#include "util/mem_utils.h"

namespace ge {
namespace {
const std::string kDisableAutoFuseNodeAttr = "_disable_autofuse_scope";
}

graphStatus CastReshapeCastPass::Run(const ComputeGraphPtr &graph) {
  for (const auto &node : graph->GetAllNodes()) {
    if ((node == nullptr) || (node->GetType() != "Reshape") || (node->GetInDataNodesSize() != 2U)) {
      continue;
    }
    const auto in_node = node->GetInDataNodes().at(0U);
    if ((in_node == nullptr) || (in_node->GetType() != "Cast")) {
      continue;
    }
    const auto in_op_desc = in_node->GetOpDesc();
    const auto in_op_in_type = in_op_desc->GetInputDesc(0).GetDataType();

    bool all_get = true;
    for (const auto &out_node : node->GetOutDataNodes()) {
      if (out_node->GetType() != "Cast") {
        all_get = false;
        break;
      }
      const auto out_op_desc = out_node->GetOpDesc();
      if (in_op_in_type != out_op_desc->GetOutputDesc(0).GetDataType()) {
        all_get = false;
        break;
      }
    }
    if (!all_get) {
      continue;
    }

    (void)AttrUtils::SetBool(in_op_desc, kDisableAutoFuseNodeAttr, true);
    (void)AttrUtils::SetBool(node->GetOpDesc(), kDisableAutoFuseNodeAttr, true);
    for (const auto &out_node : node->GetOutDataNodes()) {
      const auto out_op_desc = out_node->GetOpDesc();
      (void)AttrUtils::SetBool(out_op_desc, kDisableAutoFuseNodeAttr, true);
    }
  }
  return GRAPH_SUCCESS;
}
}  // namespace ge
