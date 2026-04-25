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

"""Python DecomposePass sample for grouped Conv2D decomposition."""

from ge.es import GraphBuilder, TensorHolder
from ge.graph import Graph, Node
from ge.passes import DecomposePass, PassStage, register_decompose_pass

try:
    from ge.es.math import Split
except ImportError:
    Split = None

try:
    from ge.es.math import Concat
except ImportError:
    Concat = None

try:
    from ge.es.math import ConcatV2
except ImportError:
    ConcatV2 = None

try:
    from ge.es.nn import Conv2DV2 as Conv2D
except ImportError:
    Conv2D = None


def _require_es_apis() -> None:
    if Split is None:
        raise RuntimeError(
            "未找到 ge.es.math.Split。请先 source CANN 环境，并确认本地 run 包中的 es_math 已正确安装。"
        )
    if Conv2D is None:
        raise RuntimeError(
            "未找到 ge.es.nn.Conv2DV2。当前样例优先复用本地 run 包中的 ES Python API，"
            "请先 source CANN 环境并确认已安装 ops 包。"
        )
    if Concat is None and ConcatV2 is None:
        raise RuntimeError(
            "未找到 ge.es.math.Concat / ConcatV2。请先 source CANN 环境，并确认本地 run 包中的 es_math 已正确安装。"
        )


def _concat_tensors(tensors: list[TensorHolder]) -> TensorHolder:
    if Concat is not None:
        return Concat(1, tensors, len(tensors))
    return ConcatV2(tensors, 1, N=len(tensors))


@register_decompose_pass(
    name="PythonDecomposeGroupedConvToSplitedPass",
    stage=PassStage.AFTER_INFER_SHAPE,
    op_types=["Conv2D"],
)
class PythonDecomposeGroupedConvToSplitedPass(DecomposePass):
    """Split grouped Conv2D into Split + Conv2D + Concat."""

    def meet_requirements(self, node: Node) -> bool:
        print("Define MeetRequirements for DecomposeGroupedConvToSplitedPass")
        try:
            groups = node.get_attr("groups")
            data_format = node.get_attr("data_format")
        except RuntimeError:
            print("Get attributes failed")
            return False
        return groups != 1 and data_format == "NCHW"

    def replacement(self, node: Node) -> Graph:
        print("Define Replacement for DecomposeGroupedConvToSplitedPass")
        try:
            groups = node.get_attr("groups")
            strides = node.get_attr("strides")
            pads = node.get_attr("pads")
            dilations = node.get_attr("dilations")
            data_format = node.get_attr("data_format")
        except RuntimeError as exc:
            raise RuntimeError("Get attributes failed") from exc

        _require_es_apis()

        builder = GraphBuilder("replacement")
        input_tensor, filter_tensor = builder.create_inputs(2)
        input_slices = Split(1, input_tensor, groups, num_split=groups)
        filter_slices = Split(0, filter_tensor, groups, num_split=groups)

        conv_outputs: list[TensorHolder] = []
        for input_slice, filter_slice in zip(input_slices, filter_slices):
            conv_outputs.append(
                Conv2D(
                    input_slice,
                    filter_slice,
                    None,
                    None,
                    strides,
                    pads,
                    dilations,
                    1,
                    data_format,
                )
            )

        result = _concat_tensors(conv_outputs)
        return builder.build_and_reset([result])


if __name__ == "__main__":
    print("PythonDecomposeGroupedConvToSplitedPass 已注册。")
    print("请先 source CANN 环境，并确认 ge.es.math / ge.es.nn 可导入，然后通过 ASCEND_GE_PY_PASS_PATH 指向本文件，例如：")
    print("  export ASCEND_GE_PY_PASS_PATH=$PWD/python/src/test_python_decompose_pass.py")
