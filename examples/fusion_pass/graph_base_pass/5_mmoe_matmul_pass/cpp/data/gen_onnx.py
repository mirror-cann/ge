#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
# ----------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------

import argparse
import logging
import os

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper


def convert():
    parser = argparse.ArgumentParser(description="Generate MMOE MatMul ONNX model")
    parser.add_argument("--m", type=int, default=64, help="rows of input x")
    parser.add_argument(
        "--k", type=int, default=128, help="reduction dimension of x and weights"
    )
    parser.add_argument(
        "--n",
        type=int,
        default=256,
        help="output dimension of weights (n > k required)",
    )
    parser.add_argument(
        "--experts", type=int, default=4, help="number of experts (MatMul nodes)"
    )
    args = parser.parse_args()

    m, k, n, experts = args.m, args.k, args.n, args.experts

    x = helper.make_tensor_value_info("x", TensorProto.FLOAT, [m, k])

    nodes = []
    outputs = []
    for i in range(experts):
        w_name = "w" + str(i + 1)
        y_name = "y" + str(i + 1)

        w_data = np.random.randn(k, n).astype(np.float32)
        w_tensor = numpy_helper.from_array(w_data, name=w_name)
        w_node = helper.make_node("Constant", [], [w_name], value=w_tensor, name=w_name)
        nodes.append(w_node)

        matmul_node = helper.make_node(
            "MatMul", ["x", w_name], [y_name], name="matmul_" + str(i + 1)
        )
        nodes.append(matmul_node)

        y = helper.make_tensor_value_info(y_name, TensorProto.FLOAT, [m, n])
        outputs.append(y)

    graph = helper.make_graph(nodes, "MmoeMatMulModel", [x], outputs)
    model = helper.make_model(graph, producer_name="mmoe_matmul_pass")
    model.opset_import[0].version = 11

    onnx.checker.check_model(model)
    output_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "model.onnx")
    onnx.save(model, output_path)
    logging.info(
        "Generated %s with shape: m=%d, k=%d, n=%d, experts=%d",
        output_path,
        m,
        k,
        n,
        experts,
    )


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    convert()
