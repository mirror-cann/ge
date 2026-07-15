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

import numpy as np
import tensorflow as tf

from tf_forward import run_benchmark

tf.compat.v1.disable_eager_execution()


def build_graph(batch, m, k, n, heads):
    x = tf.compat.v1.placeholder(tf.float32, shape=[batch, m, k * heads], name="x")
    y = tf.compat.v1.placeholder(tf.float32, shape=[1, n, k * heads], name="y")
    tile_perm = tf.constant([batch, 1, 1], dtype=tf.int32, name="tile_multiples")
    y_tiled = tf.tile(y, tile_perm, name="tiled_y")
    matmul_input0 = tf.transpose(
        tf.reshape(x, [batch, m, heads, k]), perm=[0, 2, 1, 3], name="transpose_x"
    )
    matmul_input1 = tf.transpose(
        tf.reshape(y_tiled, [batch, n, heads, k]), perm=[0, 2, 1, 3], name="transpose_y"
    )
    out = tf.matmul(matmul_input0, matmul_input1, transpose_b=True, name="output")
    return x, y, out


def run():
    parser = argparse.ArgumentParser(description="BmmTile Pattern2 online inference")
    parser.add_argument("--batch", type=int, default=100)
    parser.add_argument("--m", type=int, default=256)
    parser.add_argument("--k", type=int, default=128)
    parser.add_argument("--n", type=int, default=64)
    parser.add_argument("--heads", type=int, default=8)
    parser.add_argument("--iters", type=int, default=100)
    args = parser.parse_args()

    batch, m, k, n, heads = args.batch, args.m, args.k, args.n, args.heads

    tf.compat.v1.reset_default_graph()
    x, y, out = build_graph(batch, m, k, n, heads)

    inputs_x = np.random.randn(batch, m, k * heads).astype(np.float32)
    inputs_y = np.random.randn(1, n, k * heads).astype(np.float32)

    with tf.compat.v1.Session() as sess:
        sess.run(tf.compat.v1.global_variables_initializer())
        result_cpu = sess.run(out, feed_dict={x: inputs_x, y: inputs_y})

    from npu_bridge.npu_init import RewriterConfig

    session_config = tf.compat.v1.ConfigProto()
    optimizer = session_config.graph_options.rewrite_options.custom_optimizers.add()
    optimizer.name = "NpuOptimizer"
    optimizer.parameter_map["graph_max_parallel_model_num"].i = 1
    session_config.graph_options.rewrite_options.remapping = RewriterConfig.OFF

    with tf.compat.v1.Session(config=session_config) as sess:
        sess.run(tf.compat.v1.global_variables_initializer())
        for _ in range(10):
            sess.run(out, feed_dict={x: inputs_x, y: inputs_y})
        run_benchmark(sess, out, {x: inputs_x, y: inputs_y}, args.iters, result_cpu)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    run()
