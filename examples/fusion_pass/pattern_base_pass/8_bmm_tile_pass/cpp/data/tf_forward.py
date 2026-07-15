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
import time

import numpy as np
import tensorflow as tf

tf.compat.v1.disable_eager_execution()


def run_benchmark(sess, out, feed_dict, iters, result_cpu):
    start = time.time()
    for _ in range(iters):
        result_npu = sess.run(out, feed_dict=feed_dict)
    elapsed_ms = (time.time() - start) * 1000

    logging.info(
        "Total %d iters: %.3f ms, avg: %.4f ms",
        iters,
        elapsed_ms,
        elapsed_ms / iters,
    )
    max_err = np.max(np.abs(result_npu - result_cpu) / (np.abs(result_cpu) + 1e-10))
    logging.info("Max relative error vs CPU: %.6f", max_err)


def build_graph(batch, heads, m, k, n):
    x = tf.compat.v1.placeholder(tf.float32, shape=[1, heads, m, k], name="x")
    y = tf.compat.v1.placeholder(tf.float32, shape=[batch, heads, k, n], name="y")
    tile_multiples = tf.constant(
        [batch, 1, 1, 1], dtype=tf.int32, name="tile_multiples"
    )
    tiled_x = tf.tile(x, tile_multiples, name="tiled_x")
    out = tf.matmul(tiled_x, y, name="output")
    return x, y, out


def run():
    parser = argparse.ArgumentParser(description="BmmTile online inference")
    parser.add_argument("--batch", type=int, default=100)
    parser.add_argument("--heads", type=int, default=8)
    parser.add_argument("--m", type=int, default=256)
    parser.add_argument("--k", type=int, default=128)
    parser.add_argument("--n", type=int, default=64)
    parser.add_argument("--iters", type=int, default=100)
    args = parser.parse_args()

    batch, heads, m, k, n = args.batch, args.heads, args.m, args.k, args.n

    tf.compat.v1.reset_default_graph()
    x, y, out = build_graph(batch, heads, m, k, n)

    inputs_x = np.random.randn(1, heads, m, k).astype(np.float32)
    inputs_y = np.random.randn(batch, heads, k, n).astype(np.float32)

    with tf.compat.v1.Session() as sess:
        sess.run(tf.compat.v1.global_variables_initializer())
        result_cpu = sess.run(out, feed_dict={x: inputs_x, y: inputs_y})

    from npu_device.compat.v1.npu_init import RewriterConfig

    tf.config.optimizer.set_jit(False)
    session_config = tf.compat.v1.ConfigProto()
    optimizer = session_config.graph_options.rewrite_options.custom_optimizers.add()
    optimizer.name = "NpuOptimizer"
    session_config.graph_options.rewrite_options.remapping = RewriterConfig.OFF

    with tf.compat.v1.Session(config=session_config) as sess:
        sess.run(tf.compat.v1.global_variables_initializer())
        sess.run(out, feed_dict={x: inputs_x, y: inputs_y})
        run_benchmark(sess, out, {x: inputs_x, y: inputs_y}, args.iters, result_cpu)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    run()
