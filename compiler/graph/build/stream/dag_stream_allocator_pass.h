/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_STREAM_DAG_STREAM_ALLOCATOR_PASS_H_
#define GE_GRAPH_BUILD_STREAM_DAG_STREAM_ALLOCATOR_PASS_H_

#include <memory>
#include "external/register/register_custom_pass.h"

namespace ge {
/**
 * MiniDAG Stream Pass 核心逻辑
 * 将 GE Graph 转换为 DAG，执行流分配策略，并将结果写回 GE Graph
 *
 * @param graph GE 图对象
 * @param context Stream Pass 上下文
 * @return SUCCESS 成功，FAILED 失败
 */
Status RunMiniDAGStreamPass(const ConstGraphPtr &graph, StreamPassContext &context);
}  // namespace ge

#endif  // GE_GRAPH_BUILD_STREAM_DAG_STREAM_ALLOCATOR_PASS_H_
