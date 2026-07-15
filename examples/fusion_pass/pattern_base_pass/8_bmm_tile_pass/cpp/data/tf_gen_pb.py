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

import tensorflow as tf

tf.compat.v1.disable_eager_execution()

OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))


def build_graph(batch, heads, m, k, n):
    x = tf.compat.v1.placeholder(tf.float32, shape=[1, heads, m, k], name="x")
    y = tf.compat.v1.placeholder(tf.float32, shape=[batch, heads, k, n], name="y")
    tile_multiples = tf.constant(
        [batch, 1, 1, 1], dtype=tf.int32, name="tile_multiples"
    )
    tiled_x = tf.tile(x, tile_multiples, name="tiled_x")
    out = tf.matmul(tiled_x, y, name="output")
    return x, y, out


def freeze_and_save(output_names=None):
    if output_names is None:
        output_names = ["output"]
    config = tf.compat.v1.ConfigProto()
    config.graph_options.optimizer_options.do_constant_folding = False
    with tf.compat.v1.Session(config=config) as sess:
        sess.run(tf.compat.v1.global_variables_initializer())
        graph_def = sess.graph_def
        frozen_graph = tf.graph_util.convert_variables_to_constants(
            sess, graph_def, output_names
        )
    output_path = os.path.join(OUTPUT_DIR, "model.pb")
    with open(output_path, "wb") as f:
        f.write(frozen_graph.SerializeToString())
    return output_path


def convert():
    parser = argparse.ArgumentParser(description="Generate BmmTile TF frozen graph")
    parser.add_argument("--batch", type=int, default=100)
    parser.add_argument("--heads", type=int, default=8)
    parser.add_argument("--m", type=int, default=256)
    parser.add_argument("--k", type=int, default=128)
    parser.add_argument("--n", type=int, default=64)
    args = parser.parse_args()

    tf.compat.v1.reset_default_graph()
    build_graph(args.batch, args.heads, args.m, args.k, args.n)
    output_path = freeze_and_save()
    logging.info(
        "Generated %s (batch=%d, heads=%d, m=%d, k=%d, n=%d)",
        output_path,
        args.batch,
        args.heads,
        args.m,
        args.k,
        args.n,
    )


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    convert()
