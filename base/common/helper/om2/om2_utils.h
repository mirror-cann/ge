/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMMON_HELPER_OM2_OM2_UTILS
#define BASE_COMMON_HELPER_OM2_OM2_UTILS

#include <string>
#include "common/opskernel/ops_kernel_info_types.h"
#include "common/om2/codegen/om2_codegen_types.h"
#include "ge_common/ge_api_types.h"

namespace ge {
class Om2Utils {
 public:
  static Status CompileGeneratedCppToSo(const Om2CodegenArtifacts &artifacts, const std::string &model_name,
                                        Om2CodegenArtifact &so_artifact, const bool is_release = true);
};
}  // namespace ge

#endif  // BASE_COMMON_HELPER_OM2_OM2_UTILS
