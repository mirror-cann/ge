/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CANN_GRAPH_ENGINE_OP_COMPILE_CONTEXT_H
#define CANN_GRAPH_ENGINE_OP_COMPILE_CONTEXT_H

#include "exe_graph/runtime/extended_kernel_context.h"
#include "platform/platform_infos_def.h"

namespace gert {

class OpCompileContext : public ExtendedKernelContext {
 public:
  ge::graphStatus GetOption(const ge::AscendString &option_key, ge::AscendString &option) const;
  ge::graphStatus GetPlatformInfos(fe::PlatFormInfos &platform_info, fe::OptionalInfos& optional_infos) const;
};

}  // namespace gert
#endif  // CANN_GRAPH_ENGINE_OP_COMPILE_CONTEXT_H
