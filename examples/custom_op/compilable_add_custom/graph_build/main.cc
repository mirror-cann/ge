/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <iostream>
#include <vector>
#include "add_custom.h"
#include "graph.h"
#include "ops_proto_legacy.h"
#include "tensor.h"
#include "types.h"

using ge::Operator;

namespace ge {
bool GenGraph(Graph &graph) {
  const std::vector<int64_t> shape_data = {4, 4};
  TensorDesc desc_data(Shape(shape_data), FORMAT_ND, DT_FLOAT);

  auto data = op::Data("data");
  data.update_input_desc_x(desc_data);
  data.update_output_desc_y(desc_data);

  auto add1 = op::AddCustom("add1").set_input_x1(data).set_input_x2(data);
  auto add2 = op::AddCustom("add2").set_input_x1(add1).set_input_x2(data);

  std::vector<Operator> inputs{data};
  std::vector<Operator> outputs{add2};

  graph.SetInputs(inputs).SetOutputs(outputs);
  return graph.SaveToFile("./single_add.air");
}
}  // namespace ge

int main(int argc, char *argv[]) {
  std::cout << "========== Test Start ==========" << std::endl;
  ge::Graph graph1("GraphBuildGraph1");
  const bool ret = ge::GenGraph(graph1);
  if (ret == ge::GRAPH_SUCCESS) {
    std::cout << "========== Generate Graph1 Success! ==========" << std::endl;
    return 0;
  }
  std::cout << "========== Generate Graph1 Failed! ==========" << std::endl;
  return -1;
}
