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

from tf_forward import build_graph

tf.compat.v1.disable_eager_execution()


def convert():
    parser = argparse.ArgumentParser(
        description="Generate BMM-to-Mul-Reduce TF frozen graph"
    )
    parser.add_argument("--batch", type=int, default=100)
    parser.add_argument("--m", type=int, default=2333)
    parser.add_argument("--k", type=int, default=4)
    args = parser.parse_args()

    tf.compat.v1.reset_default_graph()
    build_graph(args.batch, args.m, args.k)

    sess_config = tf.compat.v1.ConfigProto()
    sess_config.graph_options.optimizer_options.do_constant_folding = False
    with tf.compat.v1.Session(config=sess_config) as sess:
        sess.run(tf.compat.v1.global_variables_initializer())
        frozen_graph = tf.graph_util.convert_variables_to_constants(
            sess, sess.graph_def, ["output"]
        )
    model_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "model.pb")
    with open(model_path, "wb") as f:
        f.write(frozen_graph.SerializeToString())
    logging.info(
        "Generated %s (batch=%d, m=%d, k=%d, transpose_b=True)",
        model_path,
        args.batch,
        args.m,
        args.k,
    )


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    convert()
