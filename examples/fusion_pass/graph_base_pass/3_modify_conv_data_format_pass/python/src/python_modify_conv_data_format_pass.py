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

"""Python FusionBasePass sample equivalent to ConvTransFormatPass (C++ graph_base sample)."""

from __future__ import annotations

from collections import deque

from ge.graph import Graph, Node
from ge.graph.types import DataType
from ge.passes import (
    FusionBasePass,
    PassContext,
    PassStage,
    can_fuse,
    register_fusion_pass,
    report_fuse,
)

TARGET_TYPES = frozenset({"Conv2D", "Conv2DV2"})
PERM0 = [0, 2, 3, 1]
PERM1 = [0, 3, 1, 2]

DATA_IDX = 0
PERM_IDX = 1
TRANSPOSE_OUT_IDX = 0

_CONST_LIKE_TYPES = frozenset({"Const", "Constant"})


def _flatten_to_int_list(data) -> list[int] | None:
    if data is None:
        return None
    out: list[int] = []

    def rec(x) -> None:
        if isinstance(x, (list, tuple)):
            for item in x:
                rec(item)
        else:
            out.append(int(x))

    try:
        rec(data)
    except (TypeError, ValueError):
        return None
    return out


def _read_transpose_perm_list(transpose: Node) -> list[int] | None:
    try:
        perm_node, _ = transpose.get_in_data_nodes_and_port_indexes(PERM_IDX)
    except RuntimeError:
        return None
    if perm_node.type not in _CONST_LIKE_TYPES:
        return None
    try:
        const_tensor = perm_node.get_attr("value")
    except RuntimeError:
        return None
    dtype = const_tensor.data_type
    if dtype not in (DataType.DT_INT32, DataType.DT_INT64):
        return None
    return _flatten_to_int_list(const_tensor.data)


def _judge_transpose_perm(perm: list[int], cnt_holder: list[int]) -> bool:
    c = cnt_holder[0]
    if c == 0 and perm == PERM0:
        cnt_holder[0] += 1
        return True
    if c == 1 and perm == PERM1:
        cnt_holder[0] += 1
        return True
    return False


def _remove_transpose_and_relink(
    graph: Graph, transpose_node: Node, context: PassContext
) -> bool:
    result = can_fuse([transpose_node])
    if not result.ok:
        context.set_error_message(result.reason)
        print(f"RemoveTransposeAndRelink can_fuse check failed: {result.reason}")
        return False

    data_node, data_out_idx = transpose_node.get_in_data_nodes_and_port_indexes(
        DATA_IDX
    )
    perm_node, perm_out_idx = transpose_node.get_in_data_nodes_and_port_indexes(
        PERM_IDX
    )
    consumers = list(
        transpose_node.get_out_data_nodes_and_port_indexes(TRANSPOSE_OUT_IDX)
    )
    try:
        graph.remove_edge(data_node, data_out_idx, transpose_node, DATA_IDX)
        graph.remove_edge(perm_node, perm_out_idx, transpose_node, PERM_IDX)
    except RuntimeError:
        print("Remove input edges failed")
        return False
    for out_node, out_in_idx in consumers:
        if out_node is None:
            continue
        try:
            graph.remove_edge(transpose_node, TRANSPOSE_OUT_IDX, out_node, out_in_idx)
            graph.add_data_edge(data_node, data_out_idx, out_node, out_in_idx)
        except RuntimeError:
            return False
        print("Remove output edges success")
    report_fuse([transpose_node], [], context)
    try:
        graph.remove_node(perm_node)
        graph.remove_node(transpose_node)
    except RuntimeError:
        return False
    return True


def _delete_transpose_pair_behind_if_exist(
    graph: Graph, conv_node: Node, context: PassContext
) -> bool:
    queue: deque[Node] = deque()
    transpose_cnt_holder = [0]
    out_sz = conv_node.get_outputs_size()
    for idx in range(out_sz):
        for nxt, _ in conv_node.get_out_data_nodes_and_port_indexes(idx):
            queue.append(nxt)
    while queue:
        node_ptr = queue.popleft()
        out_size = node_ptr.get_outputs_size()
        for idx in range(out_size):
            for pair in node_ptr.get_out_data_nodes_and_port_indexes(idx):
                queue.append(pair[0])
        if node_ptr.type != "Transpose":
            continue
        perm_list = _read_transpose_perm_list(node_ptr)
        if perm_list is None:
            continue
        if not _judge_transpose_perm(perm_list, transpose_cnt_holder):
            continue
        if not _remove_transpose_and_relink(graph, node_ptr, context):
            print("RemoveTransposeAndRelink failed")
            return False
        if transpose_cnt_holder[0] == 2:
            return True
    return True


def _find_nchw_conv_nodes(graph: Graph) -> list[Node]:
    conv_nodes: list[Node] = []
    for node in graph.get_direct_nodes():
        if node.type not in TARGET_TYPES:
            continue
        try:
            fmt = node.get_attr("data_format")
        except RuntimeError:
            continue
        if fmt == "NCHW":
            conv_nodes.append(node)
    return conv_nodes


@register_fusion_pass(
    name="PythonConvTransFormatPass", stage=PassStage.BEFORE_INFER_SHAPE
)
class PythonConvTransFormatPass(FusionBasePass):
    def run(self, graph: Graph, context: PassContext) -> bool:
        print("PythonConvTransFormatPass is starting")
        conv_nodes = _find_nchw_conv_nodes(graph)
        if not conv_nodes:
            print("Graph has no Conv node in NCHW format")
            return True
        for node in conv_nodes:
            try:
                node.set_attr("data_format", "NHWC")
            except RuntimeError:
                print("Modify format of node failed")
                raise
            if not _delete_transpose_pair_behind_if_exist(graph, node, context):
                print("DeleteTransposePairBehindIfExist failed")
                error_message = context.get_error_message()
                failure_message = "DeleteTransposePairBehindIfExist failed"
                if error_message:
                    failure_message += f": {error_message}"
                raise RuntimeError(failure_message)
        print("PythonConvTransFormatPass completed")
        return True


if __name__ == "__main__":
    print("PythonConvTransFormatPass 已注册。")
    print("请通过 ASCEND_GE_PY_PASS_PATH 指向本文件，例如：")
    print(
        "  export ASCEND_GE_PY_PASS_PATH=$PWD/python/src/python_modify_conv_data_format_pass.py"
    )
