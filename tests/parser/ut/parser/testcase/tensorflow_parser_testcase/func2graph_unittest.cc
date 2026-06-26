/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include "func_to_graph/func2graph.h"
#include <vector>
#include <sstream>

using namespace domi::tensorflow;

namespace ge {
class UtestFuncToGraph : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(UtestFuncToGraph, GeGraphDefSetName_invalid_value_failed) {
  GeGraphDefHandle geGraphDef = nullptr;
  GeGraphDefSetName(geGraphDef, nullptr);

  geGraphDef = GeGraphDefCreate();
  GeGraphDefSetName(geGraphDef, nullptr);

  GeGraphDefDestroy(&geGraphDef);
}

TEST_F(UtestFuncToGraph, GeGraphDefSetName_success) {
  GeGraphDefHandle geGraphDef = GeGraphDefCreate();
  const std::string graphName = "scale_func";
  GeGraphDefSetName(geGraphDef, graphName.c_str());

  auto *graph = static_cast<GeGraphDef *>(geGraphDef);
  EXPECT_EQ(graph->name(), graphName);

  GeGraphDefDestroy(&geGraphDef);
}

TEST_F(UtestFuncToGraph, GeGraphDefSetGraph_invalid_value_failed) {
  GeGraphDefHandle geGraphDef = nullptr;
  // handle is nullptr
  GeGraphDefSetGraph(geGraphDef, nullptr, 0);

  geGraphDef = GeGraphDefCreate();
  GeGraphDefSetGraph(geGraphDef, nullptr, 0);

  GeGraphDefDestroy(&geGraphDef);
}

TEST_F(UtestFuncToGraph, GeGraphDefSetGraph_success) {
  GeGraphDefHandle geGraphDef = nullptr;
  EXPECT_EQ(GeGraphDefToString(geGraphDef), nullptr);

  geGraphDef = GeGraphDefCreate();

  // create graph
  GraphDef graphDef;
  VersionDef *versionDef = new VersionDef();
  versionDef->set_producer(134);
  graphDef.set_allocated_versions((versionDef));

  int size = static_cast<int>(graphDef.ByteSizeLong());
  std::vector<uint8_t> buffer(size);
  EXPECT_TRUE(graphDef.SerializeToArray(buffer.data(), size));
  GeGraphDefSetGraph(geGraphDef, buffer.data(), size);

  const auto *graph = static_cast<GeGraphDef *>(geGraphDef);
  EXPECT_EQ(graph->graph().versions().producer(), 134);

  auto *debugString = GeGraphDefToString(geGraphDef);

  std::stringstream ss;
  ss << "graph {\n";
  ss << "  versions {\n";
  ss << "    producer: 134\n";
  ss << "  }\n";
  ss << "}\n";

  EXPECT_EQ(debugString, ss.str());

  GeGraphDefDestroy(&geGraphDef);
}

TEST_F(UtestFuncToGraph, GraphDefLibGetGraphDef_invalid_value_failed) {
  GraphDefLibHandle graphDefLib = nullptr;
  GeGraphDefHandle geGraphDef = nullptr;
  GraphDefLibAddGraphDef(graphDefLib, geGraphDef);

  constexpr int index = 0;
  EXPECT_EQ(GraphDefLibGetGraphDef(graphDefLib, index), nullptr);
}

TEST_F(UtestFuncToGraph, GraphDefLibGetGraphDef_success) {
  GraphDefLibHandle graphDefLib = nullptr;
  EXPECT_EQ(GraphDefLibGetPbtxt(graphDefLib), nullptr);

  graphDefLib = GraphDefLibCreate();
  GeGraphDefHandle geGraphDef = GeGraphDefCreate();
  GeGraphDefSetName(geGraphDef, "scale_func");
  GraphDefLibAddGraphDef(graphDefLib, geGraphDef);

  auto *pbtxt = GraphDefLibGetPbtxt(graphDefLib);
  std::stringstream ss;
  ss << "graph_def {\n";
  ss << "  name: \"scale_func\"\n";
  ss << "}\n";
  EXPECT_EQ(pbtxt, ss.str());

  constexpr int index = 0;
  EXPECT_NE(GraphDefLibGetGraphDef(graphDefLib, index), nullptr);

  GraphDefLibDestroy(&graphDefLib);
}
}  // namespace ge
