/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef PARSER_PARSER_FUNC_TO_GRAPH_FUNC2GRAPH_H
#define PARSER_PARSER_FUNC_TO_GRAPH_FUNC2GRAPH_H

#include "tensorflow/graph_library.pb.h"
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *GraphDefLibHandle;  // 对应 GraphDefLibrary
typedef void *GeGraphDefHandle;   // 对应 GeGraphDef
typedef const void *GraphDefLibConstHandle;
typedef const void *GeGraphDefConstHandle;

/**
 * Create GraphDefLibrary instance
 * @return GraphDefLibrary instance
 */
GraphDefLibHandle GraphDefLibCreate();

/**
 * Destroy GraphDefLibrary instance
 * @param handle GraphDefLibrary instance
 */
void GraphDefLibDestroy(GraphDefLibHandle *handle);

/**
 * Save GeGraphDef to GraphDefLibrary
 * @param graph_def_lib_handle GraphDefLibrary instance
 * @param graph_def_handle GeGraphDef instance. 实例所有权已转移给GraphDefLibrary，调用者无需再销毁该句柄
 */
void GraphDefLibAddGraphDef(GraphDefLibHandle graph_def_lib_handle, GeGraphDefHandle graph_def_handle);

/**
 * Get GeGraphDef from GraphDefLibrary By index
 * @param handle GraphDefLibrary instance
 * @param index GeGraphDef instance index
 * @return
 */
GeGraphDefHandle GraphDefLibGetGraphDef(GraphDefLibHandle handle, int index);

/**
 * Convert GraphDefLibrary instance to pbtxt
 * @param handle /
 * @return
 */
const char *GraphDefLibGetPbtxt(GraphDefLibConstHandle handle);

/**
 * Create GeGraphDef instance
 * @return GeGraphDef instance
 */
GeGraphDefHandle GeGraphDefCreate();

/**
 * Destroy GeGraphDef instance
 * @param handle GeGraphDef instance
 */
void GeGraphDefDestroy(GeGraphDefHandle *handle);

/**
 * Set name of GeGraphDef's attr value
 * @param handle GeGraphDef instance
 * @param name name value
 */
void GeGraphDefSetName(GeGraphDefHandle handle, const char *name);

/**
 * Set graph of GeGraphDef's attr value
 * @param handle GeGraphDef instance
 * @param data base addr of graph
 * @param len size of graph
 */
void GeGraphDefSetGraph(GeGraphDefHandle handle, const uint8_t *data, std::size_t len);

/**
 * Convert GeGraphDef instance to string for debug
 * @param handle GeGraphDef instance
 * @return
 */
const char *GeGraphDefToString(GeGraphDefConstHandle handle);

#ifdef __cplusplus
}
#endif

#endif  // PARSER_PARSER_FUNC_TO_GRAPH_FUNC2GRAPH_H
