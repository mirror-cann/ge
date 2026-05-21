/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_METADEF_GRAPH_UTILS_CG_UTILS_H_
#define GE_GRAPH_METADEF_GRAPH_UTILS_CG_UTILS_H_

// ascir/cg 定义属于 graph-autofusion 仓 namespace af::ascir::cg，GE 通过 using 引入
#include_next <utils/cg_utils.h>

namespace ge {
namespace ascir {
using af::ascir::cg::LoopOption;
using af::ascir::cg::CgContext;
using af::ascir::cg::LoopGuard;
using af::ascir::cg::BlockLoopGuard;
using af::ascir::cg::VectorizedLoopGuard;
using af::ascir::cg::Axes;
using af::ascir::cg::CodeGenUtils;
using af::ascir::cg::PadOutputViewToSched;
namespace cg = af::ascir::cg;
}  // namespace ascir
}  // namespace ge

#endif  // GE_GRAPH_METADEF_GRAPH_UTILS_CG_UTILS_H_
