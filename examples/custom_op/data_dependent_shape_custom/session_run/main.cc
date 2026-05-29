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
#include <map>
#include <memory>
#include <vector>

#include "ge/ge_api.h"
#include "graph.h"
#include "ops_proto_legacy.h"
#include "tensor.h"
#include "types.h"
#include "where_like_custom.h"

using ge::Operator;

namespace {
constexpr uint32_t kGraphId = 0U;

std::unique_ptr<ge::Graph> BuildGraph() {
  ge::TensorDesc input_desc(ge::Shape({8}), ge::FORMAT_ND, ge::DT_BOOL);
  auto data = ge::op::Data("data");
  data.update_input_desc_x(input_desc);
  data.update_output_desc_y(input_desc);

  auto where = ge::op::WhereLikeCustom("where_like").set_input_x(data);

  std::vector<Operator> inputs;
  inputs.emplace_back(data);
  std::vector<Operator> outputs;
  outputs.emplace_back(where);

  auto graph = std::make_unique<ge::Graph>("ThirdClassWhereGraph");
  graph->SetInputs(inputs).SetOutputs(outputs);
  return graph;
}

ge::Tensor BuildInputTensor() {
  ge::TensorDesc input_desc(ge::Shape({8}), ge::FORMAT_ND, ge::DT_BOOL);
  ge::Tensor input_tensor(input_desc);
  const std::vector<uint8_t> input_data = {1U, 0U, 1U, 0U, 1U, 0U, 0U, 1U};
  if (input_tensor.SetData(input_data.data(), input_data.size()) != ge::GRAPH_SUCCESS) {
    std::cerr << "SetData failed" << std::endl;
  }
  return input_tensor;
}

void PrintOutputTensor(const ge::Tensor &output_tensor) {
  const auto tensor_desc = output_tensor.GetTensorDesc();
  const auto shape = tensor_desc.GetShape();
  const auto dims = shape.GetDims();
  std::cout << "output shape: [";
  for (size_t i = 0U; i < dims.size(); ++i) {
    if (i != 0U) {
      std::cout << ", ";
    }
    std::cout << dims[i];
  }
  std::cout << "]" << std::endl;

  int64_t element_count = shape.GetShapeSize();
  if (element_count <= 0) {
    element_count = static_cast<int64_t>(output_tensor.GetSize() / sizeof(int64_t));
  }
  const auto *output_data = reinterpret_cast<const int64_t *>(output_tensor.GetData());
  std::cout << "output values:";
  for (int64_t i = 0; i < element_count; ++i) {
    std::cout << " " << output_data[i];
  }
  std::cout << std::endl;
}
}  // namespace

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  std::map<ge::AscendString, ge::AscendString> options = {
      {"ge.exec.deviceId", "0"},
      {"ge.graphRunMode", "0"},
  };

  const auto init_ret = ge::GEInitialize(options);
  if (init_ret != ge::SUCCESS) {
    std::cerr << "GEInitialize failed, ret: " << init_ret << std::endl;
    return 1;
  }

  int ret_code = 0;
  {
    ge::Session session(options);
    auto graph = BuildGraph();
    if (graph == nullptr) {
      std::cerr << "BuildGraph failed" << std::endl;
      (void)ge::GEFinalize();
      return 1;
    }
    const auto add_graph_ret = session.AddGraph(kGraphId, *graph);
    if (add_graph_ret != ge::SUCCESS) {
      std::cerr << "AddGraph failed, ret: " << add_graph_ret << std::endl;
      ret_code = 1;
    } else {
      std::vector<ge::Tensor> inputs{BuildInputTensor()};
      std::vector<ge::Tensor> outputs;
      const auto run_ret = session.RunGraph(kGraphId, inputs, outputs);
      if (run_ret != ge::SUCCESS) {
        std::cerr << "RunGraph failed, ret: " << run_ret << std::endl;
        ret_code = 1;
      } else if (outputs.empty()) {
        std::cerr << "RunGraph success but outputs is empty" << std::endl;
        ret_code = 1;
      } else {
        PrintOutputTensor(outputs[0]);
      }
      (void)session.RemoveGraph(kGraphId);
    }
  }

  const auto finalize_ret = ge::GEFinalize();
  if (finalize_ret != ge::SUCCESS) {
    std::cerr << "GEFinalize failed, ret: " << finalize_ret << std::endl;
    return 1;
  }
  return ret_code;
}
