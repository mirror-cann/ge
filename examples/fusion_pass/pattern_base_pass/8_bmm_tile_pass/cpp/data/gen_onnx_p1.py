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
# 生成 Pattern 1 (Tile -> BatchMatMulV2 直接连接) 验证用 ONNX 模型
# 使用 4D tensor + w 作为 input 避免 GE 的常量折叠消除 Tile

import argparse
import logging
import os

import onnx
from onnx import TensorProto, helper

from gen_onnx import make_tile_matmul_nodes


def convert():
    parser = argparse.ArgumentParser(
        description="Generate BmmTile Pattern1 ONNX model (4D)"
    )
    parser.add_argument("--batch", type=int, default=4)
    parser.add_argument("--m", type=int, default=64)
    parser.add_argument("--k", type=int, default=128)
    parser.add_argument("--n", type=int, default=256)
    args = parser.parse_args()

    batch, m, k, n = args.batch, args.m, args.k, args.n

    x = helper.make_tensor_value_info("x", TensorProto.FLOAT, [batch, 1, m, k])
    w = helper.make_tensor_value_info("w", TensorProto.FLOAT, [1, 1, k, n])
    y = helper.make_tensor_value_info("y", TensorProto.FLOAT, [batch, 1, m, n])

    const_repeats_node, tile_node, matmul_node = make_tile_matmul_nodes(
        [batch, 1, 1, 1]
    )

    graph = helper.make_graph(
        [const_repeats_node, tile_node, matmul_node],
        "BmmTilePattern1Model",
        [x, w],
        [y],
    )
    model = helper.make_model(graph, producer_name="bmm_tile_p1")

    model.opset_import[0].version = 11

    onnx.checker.check_model(model)
    output_path = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "model_p1.onnx"
    )
    onnx.save(model, output_path)
    logging.info(
        "Generated %s: pattern1 (Tile->BMM directly, 4D)\n"
        "  x: [%d,1,%d,%d]  w: [1,1,%d,%d] -> Tile -> [%d,1,%d,%d] -> MatMul -> [%d,1,%d,%d]\n"
        "  w is an input (not Const) to prevent GE constant folding of Tile",
        output_path,
        batch,
        m,
        k,
        k,
        n,
        batch,
        k,
        n,
        batch,
        m,
        n,
    )


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    convert()
