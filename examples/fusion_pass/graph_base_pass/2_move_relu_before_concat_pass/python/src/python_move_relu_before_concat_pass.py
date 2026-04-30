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

"""Python FusionBasePass sample equivalent to MoveReluBeforeConcatPass (C++ graph_base sample)."""

from __future__ import annotations

from ge.es import GraphBuilder, TensorHolder
from ge.graph import Graph, Node
from ge.passes import FusionBasePass, PassStage, register_fusion_pass, SubgraphBoundary, SubgraphInput, SubgraphOutput, SubgraphRewriter

try:
    from ge.es.math import ConcatV2
except ImportError:
    ConcatV2 = None

try:
    from ge.es.nn import Relu
except ImportError:
    Relu = None


def _require_es_apis() -> None:
    if ConcatV2 is None:
        raise RuntimeError(
            "未找到 ge.es.math.ConcatV2。请先 source CANN 环境，并确认本地 run 包中的 es_math 已正确安装。"
        )
    if Relu is None:
        raise RuntimeError(
            "未找到 ge.es.nn.Relu。请先 source CANN 环境，并确认本地 run 包中的 es_nn 已正确安装。"
        )


def _is_node_fed_into_relu(node: Node) -> bool:
    if node.get_outputs_size() != 1:
        return False
    out_list = node.get_out_data_nodes_and_port_indexes(0)
    if not out_list:
        return False
    out_node, _ = out_list[0]
    return out_node.type == "Relu"


def _find_concat_nodes(graph: Graph) -> list[Node]:
    concat_nodes: list[Node] = []
    for node in graph.get_direct_nodes():
        if node.type == "ConcatV2" and _is_node_fed_into_relu(node):
            concat_nodes.append(node)
    return concat_nodes


def _build_replacement_graph(concat_node: Node) -> Graph:
    _require_es_apis()

    builder = GraphBuilder("replacement")
    input_size = concat_node.get_inputs_size()
    if input_size < 2:
        raise RuntimeError("ConcatV2 inputs_size is too small")

    relued: list[TensorHolder] = []
    for idx in range(input_size - 1):
        x = builder.create_input(idx)
        relued.append(Relu(x))

    concat_dim = builder.create_input(input_size - 1)
    out = ConcatV2(relued, concat_dim, N=len(relued))
    return builder.build_and_reset([out])


def _build_boundary(concat_node: Node) -> SubgraphBoundary:
    boundary = SubgraphBoundary()
    input_size = concat_node.get_inputs_size()
    for input_idx in range(input_size):
        subgraph_input = SubgraphInput()
        # Boundary input i corresponds to ConcatV2 input port i
        subgraph_input.add_input(concat_node, int(input_idx))
        boundary.add_input(int(input_idx), subgraph_input)

    out_list = concat_node.get_out_data_nodes_and_port_indexes(0)
    if not out_list:
        raise RuntimeError("ConcatV2 has no out data nodes")
    relu_node, _ = out_list[0]
    if relu_node.type != "Relu":
        raise RuntimeError("ConcatV2 output is not fed into Relu")

    subgraph_output = SubgraphOutput(relu_node, 0)
    boundary.add_output(0, subgraph_output)
    return boundary


@register_fusion_pass(name="PythonMoveReluBeforeConcatPass", stage=PassStage.BEFORE_INFER_SHAPE)
class PythonMoveReluBeforeConcatPass(FusionBasePass):
    def run(self, graph: Graph, context) -> bool:
        print("PythonMoveReluBeforeConcatPass")

        concat_nodes = _find_concat_nodes(graph)
        if not concat_nodes:
            return True

        for concat_node in concat_nodes:
            replacement = _build_replacement_graph(concat_node)
            boundary = _build_boundary(concat_node)
            ret = SubgraphRewriter.replace(boundary, replacement)
            if ret != 0:
                raise RuntimeError(f"SubgraphRewriter.replace failed, ret={ret}")
            print("Replacement of PythonMoveReluBeforeConcatPass succeeded")
        return True


if __name__ == "__main__":
    print("PythonMoveReluBeforeConcatPass 已注册。")
    print("请通过 ASCEND_GE_PY_PASS_PATH 指向本文件，例如：")
    print("  export ASCEND_GE_PY_PASS_PATH=$PWD/python/src/python_move_relu_before_concat_pass.py")

