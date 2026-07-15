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

import tensorflow as tf

from tf_forward_p2 import build_graph
from tf_gen_pb import freeze_and_save

tf.compat.v1.disable_eager_execution()


def convert():
    parser = argparse.ArgumentParser(
        description="Generate BmmTile Pattern2 TF frozen graph"
    )
    parser.add_argument("--batch", type=int, default=100)
    parser.add_argument("--m", type=int, default=256)
    parser.add_argument("--k", type=int, default=128)
    parser.add_argument("--n", type=int, default=64)
    parser.add_argument("--heads", type=int, default=8)
    args = parser.parse_args()

    tf.compat.v1.reset_default_graph()
    build_graph(args.batch, args.m, args.k, args.n, args.heads)
    output_path = freeze_and_save()
    logging.info(
        "Generated %s (batch=%d, m=%d, k=%d, n=%d, heads=%d, tile_on=input1)",
        output_path,
        args.batch,
        args.m,
        args.k,
        args.n,
        args.heads,
    )


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    convert()
