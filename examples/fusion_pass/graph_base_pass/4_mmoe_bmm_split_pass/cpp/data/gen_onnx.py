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
from dataclasses import dataclass

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper


@dataclass
class ModelConfig:
    m: int
    k1: int
    n1: int
    n2: int
    experts: int


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate MMOE 2-layer MatMul ONNX model"
    )
    parser.add_argument("--m", type=int, default=64, help="rows of input x")
    parser.add_argument(
        "--k1", type=int, default=128, help="reduction dimension of layer-1 weights"
    )
    parser.add_argument(
        "--n1", type=int, default=256, help="output dimension of layer-1 weights"
    )
    parser.add_argument(
        "--n2", type=int, default=128, help="output dimension of layer-2 weights"
    )
    parser.add_argument(
        "--experts",
        type=int,
        default=4,
        help="number of experts (MatMul nodes per layer)",
    )
    return parser.parse_args()


def build_layer1(nodes, cfg):
    # Layer 1: x @ w1_i -> relu_i  (matmul_level=1, NOT merged by this pass)
    h1_names = []
    for i in range(cfg.experts):
        w1_name = "w1_" + str(i + 1)
        h1_name = "h1_" + str(i + 1)

        w1_data = np.random.randn(cfg.k1, cfg.n1).astype(np.float32)
        w1_tensor = numpy_helper.from_array(w1_data, name=w1_name)
        w1_node = helper.make_node(
            "Constant", [], [w1_name], value=w1_tensor, name=w1_name
        )
        nodes.append(w1_node)

        matmul1_name = "matmul1_" + str(i + 1)
        matmul1_node = helper.make_node(
            "MatMul", ["x", w1_name], [h1_name], name=matmul1_name
        )
        nodes.append(matmul1_node)

        relu_name = "relu_" + str(i + 1)
        relu_out = "relu_out_" + str(i + 1)
        relu_node = helper.make_node("Relu", [h1_name], [relu_out], name=relu_name)
        nodes.append(relu_node)
        h1_names.append(relu_out)
    return h1_names


def build_layer2(nodes, outputs, h1_names, cfg):
    # Layer 2: relu_i @ w2_i -> y_i  (matmul_level=2, merged by this pass)
    for i in range(cfg.experts):
        w2_name = "w2_" + str(i + 1)
        y_name = "y" + str(i + 1)

        w2_data = np.random.randn(cfg.n1, cfg.n2).astype(np.float32)
        w2_tensor = numpy_helper.from_array(w2_data, name=w2_name)
        w2_node = helper.make_node(
            "Constant", [], [w2_name], value=w2_tensor, name=w2_name
        )
        nodes.append(w2_node)

        matmul2_name = "matmul2_" + str(i + 1)
        matmul2_node = helper.make_node(
            "MatMul", [h1_names[i], w2_name], [y_name], name=matmul2_name
        )
        nodes.append(matmul2_node)

        y = helper.make_tensor_value_info(y_name, TensorProto.FLOAT, [cfg.m, cfg.n2])
        outputs.append(y)


def save_model(nodes, x, outputs, cfg):
    graph = helper.make_graph(nodes, "MmoeBmmSplitModel", [x], outputs)
    model = helper.make_model(graph, producer_name="mmoe_bmm_split_pass")
    model.opset_import[0].version = 11

    onnx.checker.check_model(model)
    output_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "model.onnx")
    onnx.save(model, output_path)
    logging.info(
        "Generated %s with shape: m=%d, k1=%d, n1=%d, n2=%d, experts=%d",
        output_path,
        cfg.m,
        cfg.k1,
        cfg.n1,
        cfg.n2,
        cfg.experts,
    )


def convert():
    args = parse_args()
    cfg = ModelConfig(
        m=args.m, k1=args.k1, n1=args.n1, n2=args.n2, experts=args.experts
    )

    x = helper.make_tensor_value_info("x", TensorProto.FLOAT, [cfg.m, cfg.k1])
    nodes = []
    outputs = []

    h1_names = build_layer1(nodes, cfg)
    build_layer2(nodes, outputs, h1_names, cfg)
    save_model(nodes, x, outputs, cfg)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    convert()
