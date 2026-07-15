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


def build_graph(batch, m, k):
    x = tf.compat.v1.placeholder(tf.float32, shape=[batch, m, k], name="x")
    w = tf.constant(
        np.random.randn(batch, 1, k).astype(np.float32),
        dtype=tf.float32,
        shape=[batch, 1, k],
        name="weight",
    )
    y = tf.matmul(x, w, transpose_b=True, name="output")
    return x, y


def run():
    parser = argparse.ArgumentParser(description="BMM-to-Mul-Reduce online inference")
    parser.add_argument("--batch", type=int, default=100)
    parser.add_argument("--m", type=int, default=2333)
    parser.add_argument("--k", type=int, default=4)
    parser.add_argument("--iters", type=int, default=100)
    args = parser.parse_args()

    batch, m, k = args.batch, args.m, args.k

    tf.compat.v1.reset_default_graph()
    x, y = build_graph(batch, m, k)

    inputs_x = np.random.randn(batch, m, k).astype(np.float32)

    with tf.compat.v1.Session() as sess:
        sess.run(tf.compat.v1.global_variables_initializer())
        result_cpu = sess.run(y, feed_dict={x: inputs_x})

    from npu_device.compat.v1.npu_init import RewriterConfig

    tf.config.optimizer.set_jit(False)
    session_config = tf.compat.v1.ConfigProto()
    optimizer = session_config.graph_options.rewrite_options.custom_optimizers.add()
    optimizer.name = "NpuOptimizer"
    session_config.graph_options.rewrite_options.remapping = RewriterConfig.OFF

    with tf.compat.v1.Session(config=session_config) as sess:
        sess.run(tf.compat.v1.global_variables_initializer())
        sess.run(y, feed_dict={x: inputs_x})

        start = time.time()
        for _ in range(args.iters):
            result_npu = sess.run(y, feed_dict={x: inputs_x})
        elapsed_ms = (time.time() - start) * 1000

    logging.info(
        "Total %d iters: %.3f ms, avg: %.4f ms",
        args.iters,
        elapsed_ms,
        elapsed_ms / args.iters,
    )
    max_err = np.max(np.abs(result_npu - result_cpu) / (np.abs(result_cpu) + 1e-10))
    logging.info("Max relative error vs CPU: %.6f", max_err)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    run()
