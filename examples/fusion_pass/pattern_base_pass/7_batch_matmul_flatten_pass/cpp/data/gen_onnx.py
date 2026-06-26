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

import onnx
from onnx import TensorProto, helper


def convert():
    parser = argparse.ArgumentParser(description="Generate BatchMatMul ONNX model")
    parser.add_argument("--batch", type=int, default=32)
    parser.add_argument("--m", type=int, default=64)
    parser.add_argument("--k", type=int, default=512)
    parser.add_argument("--n", type=int, default=256)
    args = parser.parse_args()

    batch, m, k, n = args.batch, args.m, args.k, args.n

    x = helper.make_tensor_value_info("x", TensorProto.FLOAT, [batch, m, k])
    w = helper.make_tensor_value_info("w", TensorProto.FLOAT, [k, n])
    y = helper.make_tensor_value_info("y", TensorProto.FLOAT, [batch, m, n])

    matmul_node = helper.make_node("MatMul", inputs=["x", "w"], outputs=["y"], name="batch_matmul")

    graph = helper.make_graph([matmul_node], "BatchMatMulModel", [x, w], [y])
    model = helper.make_model(graph, producer_name="batch_matmul_flatten_pass")

    model.opset_import[0].version = 11

    onnx.checker.check_model(model)
    output_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "model.onnx")
    onnx.save(model, output_path)
    logging.info(
        "Generated %s with shape: batch=%d, m=%d, k=%d, n=%d",
        output_path,
        batch,
        m,
        k,
        n,
    )


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    convert()
