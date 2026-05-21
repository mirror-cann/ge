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
from ge.utils import GeUtils
from ge.graph import DataType, Format, Graph, Node, TensorDesc
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
    from ge.es.nn import Conv2D
except ImportError:
    try:
        from ge.es.all import Conv2D
    except ImportError:
        Conv2D = None


def _require_es_apis() -> None:
    if Split is None:
        raise RuntimeError(
            "未找到 ge.es.math.Split。请先 source CANN 环境，并确认本地 run 包中的 es_math 已正确安装。"
        )
    if Conv2D is None:
        raise RuntimeError(
            "未找到 ge.es.nn.Conv2D / ge.es.all.Conv2D。当前样例优先复用本地 run 包中的 ES Python API，"
            "请先 source CANN 环境并确认已安装 ops 包或确保 es_all 已正确生成并加载。"
        )
    if Concat is None and ConcatV2 is None:
        raise RuntimeError(
            "未找到 ge.es.math.Concat / ConcatV2。请先 source CANN 环境，并确认本地 run 包中的 es_math 已正确安装。"
        )


def _concat_tensors(tensors: list[TensorHolder]) -> TensorHolder:
    if Concat is not None:
        return Concat(1, tensors, N=len(tensors))
    return ConcatV2(tensors, 1, N=len(tensors))


def _infer_shape_and_check_support(matched_node: Node, graph: Graph) -> bool:
    input_shapes = []
    for input_index in range(matched_node.get_inputs_size()):
        input_shapes.append(matched_node.get_input_desc(input_index).get_shape())

    try:
        GeUtils.infer_shape(graph, input_shapes)
    except RuntimeError as exc:
        print(f"InferShape failed, reason: {exc}")
        return False

    for graph_node in graph.get_all_nodes():
        node_type = graph_node.type
        if node_type in ("Const", "Data"):
            continue
        try:
            is_supported, unsupported_reason = GeUtils.check_node_support_on_aicore(graph_node)
        except RuntimeError as exc:
            print(f"CheckNodeSupportOnAicore failed, reason: {exc}")
            return False
        if not is_supported:
            print(
                f"Node is not supported on current ai core! reason: {unsupported_reason} "
                f"node name: {graph_node.name} node type: {node_type}"
            )
            return False
    return True


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
        except RuntimeError as exc:
            raise RuntimeError("Get attributes failed") from exc

        _require_es_apis()

        builder = GraphBuilder("replacement")
        input_tensor, filter_tensor = builder.create_inputs(2)
        input_slices = Split(1, input_tensor, groups, num_split=groups)
        filter_slices = Split(0, filter_tensor, groups, num_split=groups)

        conv_outputs: list[TensorHolder] = []
        for input_slice, filter_slice in zip(input_slices, filter_slices):
            r_conv = Conv2D(
                input_slice,
                filter_slice,
                None,
                None,
                strides=strides,
                pads=pads,
                dilations=dilations,
                groups=1,
                data_format="NCHW",
            )

            desc = TensorDesc([], Format.FORMAT_NCHW, DataType.DT_FLOAT)
            desc.set_origin_format(Format.FORMAT_NCHW)
            conv_node = r_conv._get_node_snapshot()
            conv_node.update_input_desc(0, desc)
            conv_node.update_input_desc(1, desc)
            conv_node.update_output_desc(0, desc)
            conv_outputs.append(r_conv)

        result = _concat_tensors(conv_outputs)
        replacement_graph = builder.build_and_reset([result])
        if not _infer_shape_and_check_support(node, replacement_graph):
            raise RuntimeError("InferShape or CheckNodeSupportOnAicore failed")

        return replacement_graph


if __name__ == "__main__":
    print("PythonDecomposeGroupedConvToSplitedPass 已注册。")
    print("请先 source CANN 环境并生成 es_all，确认 ge.es.math / ge.es.nn / ge.es.all 可导入，然后通过 ASCEND_GE_PY_PASS_PATH 指向本文件，例如：")
    print("  export ASCEND_GE_PY_PASS_PATH=$PWD/src/python_decompose_pass.py")
