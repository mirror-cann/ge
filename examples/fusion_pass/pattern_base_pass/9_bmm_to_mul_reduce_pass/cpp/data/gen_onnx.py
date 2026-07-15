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


def convert():
    parser = argparse.ArgumentParser(
        description="Generate BMM-to-Mul-Reduce ONNX model"
    )
    parser.add_argument("--batch", type=int, default=100)
    parser.add_argument("--m", type=int, default=2333)
    parser.add_argument("--k", type=int, default=4)
    args = parser.parse_args()

    batch, m, k = args.batch, args.m, args.k

    x = helper.make_tensor_value_info("x", TensorProto.FLOAT, [batch, m, k])
    y = helper.make_tensor_value_info("y", TensorProto.FLOAT, [batch, m, 1])

    w_value = np.random.randn(batch, 1, k).astype(np.float32)
    w_tensor = helper.make_tensor(
        "w_value",
        TensorProto.FLOAT,
        [batch, 1, k],
        w_value.flatten().tolist(),
    )
    w_const = helper.make_node("Constant", [], ["w"], value=w_tensor, name="weight")

    transpose_node = helper.make_node(
        "Transpose", ["w"], ["w_t"], perm=[0, 2, 1], name="transpose_w"
    )

    matmul_node = helper.make_node("MatMul", ["x", "w_t"], ["y"], name="batch_matmul")

    graph = helper.make_graph(
        [w_const, transpose_node, matmul_node], "BmmToMulReduceModel", [x], [y]
    )
    model = helper.make_model(graph, producer_name="bmm_to_mul_reduce_pass")

    model.opset_import[0].version = 11

    onnx.checker.check_model(model)
    output_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "model.onnx")
    onnx.save(model, output_path)
    logging.info(
        "Generated %s with shape: batch=%d, m=%d, k=%d",
        output_path,
        batch,
        m,
        k,
    )


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    convert()
