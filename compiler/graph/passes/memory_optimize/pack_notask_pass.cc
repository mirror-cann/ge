/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/passes/memory_optimize/pack_notask_pass.h"
#include "graph/utils/type_utils.h"
#include "graph/utils/type_utils_inner.h"
#include "common/checker.h"

namespace ge {
// Pack算子仅支持ND（公有格式）和FRACTAL_NZ（私有格式，含C0变体），其他私有格式不做notask优化
// 注意：format 可能携带 sub_format / c0_format 复合位（如 FRACTAL_NZ_C0_16），整值比较会漏判，
// 必须按 GetPrimaryFormat 归一后再判断主格式，与 IsInternalFormat 内部口径保持一致。
// 后续若需新增支持格式需要另外分析
bool PackNotaskPass::CheckFormat(const ge::OpDescPtr &op_desc) const {
  for (size_t i = 0UL; i < op_desc->GetAllInputsSize(); ++i) {
    const auto format = op_desc->GetInputDesc(i).GetFormat();
    const auto primary_format = static_cast<ge::Format>(ge::GetPrimaryFormat(static_cast<int32_t>(format)));
    if (TypeUtilsInner::IsInternalFormat(format) && (primary_format != FORMAT_FRACTAL_NZ)) {
      GELOGD(
          "Pack [%s] input[%zu] format %s (primary %s) is unsupported internal format, "
          "only FRACTAL_NZ (and its C0 variants) is allowed.",
          op_desc->GetName().c_str(), i, ge::TypeUtils::FormatToSerialString(format).c_str(),
          ge::TypeUtils::FormatToSerialString(primary_format).c_str());
      return false;
    }
  }
  return true;
}

// Pack的axis作用于原始输入shape，判断axis前的轴是否全为1
bool PackNotaskPass::CheckDim(const ge::OpDescPtr &op_desc) const {
  int64_t pack_dim = 0;

  (void)ge::AttrUtils::GetInt(op_desc, "axis", pack_dim);
  for (size_t i = 0UL; i < op_desc->GetAllInputsSize(); ++i) {
    ge::GeShape input_orinal_shape = op_desc->GetInputDesc(i).GetOriginShape();

    if (pack_dim < 0) {
      GELOGD("Pack_dim[%lld] is negative number, change it to positive.", pack_dim);
      pack_dim = static_cast<int64_t>(input_orinal_shape.GetDimNum()) + 1 + pack_dim;
    }
    // Pack的axis范围是[0, rank]，axis==rank表示在最后插入新维度
    GE_ASSERT_TRUE((pack_dim >= 0) && (static_cast<size_t>(pack_dim) <= input_orinal_shape.GetDimNum()));

    // axis==0时，前面没有轴，天然满足条件
    if (pack_dim == 0) {
      continue;
    }

    // axis==rank时新维度在末尾，所有原始维度都为1时才可优化（单元素tensor无交错）
    // 不走CheckDimForInput是因为axis=rank在输入shape的轴映射中无对应条目，传入会越界
    // 此处直接检查所有原始维度为1，已覆盖CheckDimForInput中的两项校验：
    //   - CheckRealDim：要求"axis前维度为1"，axis=rank时"前"即全部维度，此处条件更强
    //   - CheckDimAlignment：检查ori_shape[axis]的padding对齐，axis=rank指向输出新增的N维，输入中不存在，无需检查
    if (static_cast<size_t>(pack_dim) == input_orinal_shape.GetDimNum()) {
      for (size_t d = 0; d < input_orinal_shape.GetDimNum(); ++d) {
        if (input_orinal_shape.GetDim(d) != 1) {
          GELOGD("Pack [%s] axis==rank but dim[%zu]=%lld != 1, interleaved layout cannot be no-task optimized.",
                 op_desc->GetName().c_str(), d, input_orinal_shape.GetDim(d));
          return false;
        }
      }
      continue;
    }

    if (!CheckDimForInput(op_desc, pack_dim, i)) {
      return false;
    }
  }
  return true;
}

REG_PASS_OPTION("PackNotaskPass").LEVELS(OoLevel::kO3);
}  // namespace ge
