/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "ge/ge_ir_build.h"
#include "ge_api_c_wrapper_utils.h"
#include "graph/graph.h"

#include <map>
#include <new>
#include <utility>
#include <vector>

using namespace ge;
using namespace ge::c_wrapper;
namespace {
graphStatus BuildOptionMapFromArrays(char **keys, char **values, const int size,
                                     std::map<AscendString, AscendString> &options) {
  if (size == 0) {
    return GRAPH_SUCCESS;
  }
  GE_ASSERT_NOTNULL(keys);
  GE_ASSERT_NOTNULL(values);
  for (int i = 0; i < size; i++) {
    GE_ASSERT_NOTNULL(keys[i]);
    GE_ASSERT_NOTNULL(values[i]);
    options.emplace(keys[i], values[i]);
  }
  return GRAPH_SUCCESS;
}
}  // namespace

#ifdef __cplusplus
extern "C" {
#endif

graphStatus GeApiWrapper_OfflineCompile_BuildInitialize(char **keys, char **values, int size) {
  std::map<AscendString, AscendString> options;
  const auto ret = BuildOptionMapFromArrays(keys, values, size, options);
  if (ret != GRAPH_SUCCESS) {
    return ret;
  }

  return aclgrphBuildInitialize(options);
}

void GeApiWrapper_OfflineCompile_BuildFinalize() {
  aclgrphBuildFinalize();
}

graphStatus GeApiWrapper_OfflineCompile_BuildModel(Graph *graph, char **keys, char **values, int size,
                                                          ModelBufferData **model) {
  GE_ASSERT_NOTNULL(graph);
  GE_ASSERT_NOTNULL(model);
  *model = nullptr;

  std::map<AscendString, AscendString> options;
  const auto option_ret = BuildOptionMapFromArrays(keys, values, size, options);
  if (option_ret != GRAPH_SUCCESS) {
    return option_ret;
  }

  auto *model_data = new (std::nothrow) ModelBufferData();
  if (model_data == nullptr) {
    return GRAPH_FAILED;
  }
  const auto ret = aclgrphBuildModel(*graph, options, *model_data);
  if (ret != GRAPH_SUCCESS) {
    delete model_data;
    return ret;
  }
  *model = model_data;
  return GRAPH_SUCCESS;
}

graphStatus GeApiWrapper_OfflineCompile_SaveModel(const char_t *output_file, const ModelBufferData *model) {
  GE_ASSERT_NOTNULL(output_file);
  GE_ASSERT_NOTNULL(model);
  return aclgrphSaveModel(output_file, *model);
}

graphStatus GeApiWrapper_OfflineCompile_BundleBuildModel(Graph **graphs, char ***keys, char ***values,
                                                                int *sizes, int graph_count, ModelBufferData **model) {
  GE_ASSERT_NOTNULL(graphs);
  GE_ASSERT_NOTNULL(keys);
  GE_ASSERT_NOTNULL(values);
  GE_ASSERT_NOTNULL(sizes);
  GE_ASSERT_NOTNULL(model);
  *model = nullptr;

  std::vector<GraphWithOptions> graph_with_options;
  graph_with_options.reserve(static_cast<size_t>(graph_count));
  for (int i = 0; i < graph_count; ++i) {
    GE_ASSERT_NOTNULL(graphs[i]);
    std::map<AscendString, AscendString> options;
    const auto option_ret = BuildOptionMapFromArrays(keys[i], values[i], sizes[i], options);
    if (option_ret != GRAPH_SUCCESS) {
      return option_ret;
    }
    graph_with_options.push_back(GraphWithOptions{*graphs[i], std::move(options)});
  }

  auto *model_data = new (std::nothrow) ModelBufferData();
  GE_ASSERT_NOTNULL(model_data);
  const auto ret = aclgrphBundleBuildModel(graph_with_options, *model_data);
  if (ret != GRAPH_SUCCESS) {
    delete model_data;
    return ret;
  }
  *model = model_data;
  return GRAPH_SUCCESS;
}

graphStatus GeApiWrapper_OfflineCompile_BundleSaveModel(const char_t *output_file,
                                                               const ModelBufferData *model) {
  GE_ASSERT_NOTNULL(output_file);
  GE_ASSERT_NOTNULL(model);
  return aclgrphBundleSaveModel(output_file, *model);
}

void GeApiWrapper_ModelBuffer_Destroy(const ModelBufferData *model) {
  delete model;
}

uint64_t GeApiWrapper_ModelBuffer_GetLength(const ModelBufferData *model) {
  GE_ASSERT_NOTNULL(model);
  return model->length;
}

#ifdef __cplusplus
}
#endif
