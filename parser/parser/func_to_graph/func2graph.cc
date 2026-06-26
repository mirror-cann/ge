/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "func2graph.h"
#include "tensorflow/graph_library.pb.h"
#include <google/protobuf/text_format.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOGE(format, ...) printf("[ERROR] %s:%d " format "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) printf("[INFO] %s:%d " format "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define PROTOBUF_MAX_SIZE (2UL * 1024 * 1024 * 1024)

using namespace domi::tensorflow;

/**
 * Create GraphDefLibrary instance
 * @return GraphDefLibrary instance
 */
GraphDefLibHandle GraphDefLibCreate() {
  return new GraphDefLibrary();
}

/**
 * Destroy GraphDefLibrary instance
 * @param handle GraphDefLibrary instance
 */
void GraphDefLibDestroy(GraphDefLibHandle *handle) {
  if (handle != nullptr) {
    delete static_cast<GraphDefLibrary *>(*handle);
    *handle = nullptr;
  }
}

/**
 * Save GeGraphDef to GraphDefLibrary
 * @param graph_def_lib_handle GraphDefLibrary instance
 * @param graph_def_handle GeGraphDef instance
 */
void GraphDefLibAddGraphDef(GraphDefLibHandle graph_def_lib_handle, GeGraphDefHandle graph_def_handle) {
  if (graph_def_lib_handle == nullptr || graph_def_handle == nullptr) {
    LOGE("existed nullptr, graph_def_lib_handle = %p, graph_def_handle = %p", graph_def_lib_handle, graph_def_handle);
    return;
  }

  auto *library = static_cast<GraphDefLibrary *>(graph_def_lib_handle);
  auto *graph = static_cast<GeGraphDef *>(graph_def_handle);
  library->mutable_graph_def()->AddAllocated(graph);
}

/**
 * Get GeGraphDef from GraphDefLibrary By index
 * @param handle GraphDefLibrary instance
 * @param index GeGraphDef instance index
 * @return
 */
GeGraphDefHandle GraphDefLibGetGraphDef(GraphDefLibHandle handle, int index) {
  if (handle == nullptr) {
    LOGE("handle is nullptr");
    return nullptr;
  }

  auto *library = static_cast<GraphDefLibrary *>(handle);
  if (index < 0 || index >= library->graph_def_size()) {
    LOGE("index out of range, graph_def_size is %d", library->graph_def_size());
    return nullptr;
  }

  return library->mutable_graph_def(index);
}

/**
 * Convert GraphDefLibrary instance to pbtxt
 * @param handle /
 * @return
 */
const char *GraphDefLibGetPbtxt(GraphDefLibConstHandle handle) {
  if (handle == nullptr) {
    LOGE("handle is nullptr");
    return nullptr;
  }

  static thread_local std::string pbtxt;

  const auto *library = static_cast<const GraphDefLibrary *>(handle);
  google::protobuf::TextFormat::PrintToString(*library, &pbtxt);
  return pbtxt.c_str();
}

/**
 * Create GeGraphDef instance
 * @return GeGraphDef instance
 */
GeGraphDefHandle GeGraphDefCreate() {
  return new GeGraphDef();
}

/**
 * Destroy GeGraphDef instance
 * @param handle GeGraphDef instance
 */
void GeGraphDefDestroy(GeGraphDefHandle *handle) {
  if (handle != nullptr) {
    delete static_cast<GeGraphDef *>(*handle);
    *handle = nullptr;
  }
}

/**
 * Set name of GeGraphDef's attr value
 * @param handle GeGraphDef instance
 * @param name name value
 */
void GeGraphDefSetName(GeGraphDefHandle handle, const char *name) {
  if (handle == nullptr) {
    LOGE("handle is nullptr");
    return;
  }

  if (name == nullptr) {
    LOGE("name is nullptr");
    return;
  }

  auto *graph = static_cast<GeGraphDef *>(handle);
  graph->set_name(name);
}

/**
 * Set graph of GeGraphDef's attr value
 * @param handle GeGraphDef instance
 * @param data base addr of graph
 * @param len size of graph
 */
void GeGraphDefSetGraph(GeGraphDefHandle handle, const uint8_t *data, std::size_t len) {
  if (handle == nullptr) {
    LOGE("handle is nullptr");
    return;
  }

  if (data == nullptr || len == 0 || len > PROTOBUF_MAX_SIZE) {
    LOGI("graph is invalid, data:[%p], len:[%zu]", data, len);
    return;
  }

  auto *graph = static_cast<GeGraphDef *>(handle);
  graph->mutable_graph()->ParseFromArray(data, static_cast<int>(len));
}

/**
 * Convert GeGraphDef instance to string for debug
 * @param handle GeGraphDef instance
 * @return
 */
const char *GeGraphDefToString(GeGraphDefConstHandle handle) {
  if (handle == nullptr) {
    LOGE("handle is nullptr");
    return nullptr;
  }

  static thread_local std::string text;
  const auto *graph = static_cast<const GeGraphDef *>(handle);

  text = graph->DebugString();
  return text.c_str();
}

#ifdef __cplusplus
}
#endif
