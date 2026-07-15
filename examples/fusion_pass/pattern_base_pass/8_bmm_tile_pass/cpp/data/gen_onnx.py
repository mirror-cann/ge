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
from onnx import TensorProto, helper


def make_tile_matmul_nodes(repeats_values):
    repeats_data = np.array(repeats_values, dtype=np.int64)
    repeats_tensor = helper.make_tensor(
        "repeats",
        TensorProto.INT64,
        [len(repeats_values)],
        repeats_data.tobytes(),
        raw=True,
    )
    const_repeats_node = helper.make_node(
        "Constant", [], ["repeats"], value=repeats_tensor, name="repeats_const"
    )
    tile_node = helper.make_node("Tile", ["w", "repeats"], ["tiled_w"], name="tile")
    matmul_node = helper.make_node(
        "MatMul", ["x", "tiled_w"], ["y"], name="batch_matmul"
    )
    return const_repeats_node, tile_node, matmul_node


def convert():
    parser = argparse.ArgumentParser(description="Generate BmmTile ONNX model")
    parser.add_argument("--batch", type=int, default=2)
    parser.add_argument("--m", type=int, default=64)
    parser.add_argument("--k", type=int, default=128)
    parser.add_argument("--n", type=int, default=256)
    args = parser.parse_args()

    batch, m, k, n = args.batch, args.m, args.k, args.n

    x = helper.make_tensor_value_info("x", TensorProto.FLOAT, [batch, m, k])
    y = helper.make_tensor_value_info("y", TensorProto.FLOAT, [batch, m, n])

    w = helper.make_tensor_value_info("w", TensorProto.FLOAT, [1, k, n])

    const_repeats_node, tile_node, matmul_node = make_tile_matmul_nodes([batch, 1, 1])

    graph = helper.make_graph(
        [const_repeats_node, tile_node, matmul_node],
        "BmmTileModel",
        [x, w],
        [y],
    )
    model = helper.make_model(graph, producer_name="bmm_tile_pass")

    model.opset_import[0].version = 11

    onnx.checker.check_model(model)
    output_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "model.onnx")
    onnx.save(model, output_path)
    logging.info(
        "Generated %s with shape: batch=%d, m=%d, k=%d, n=%d (w: [1, %d, %d] -> tile -> [%d, %d, %d])",
        output_path,
        batch,
        m,
        k,
        n,
        k,
        n,
        batch,
        k,
        n,
    )


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    convert()
