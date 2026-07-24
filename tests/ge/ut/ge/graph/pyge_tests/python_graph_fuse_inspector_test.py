#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

"""Pytest coverage for the Python graph fuse inspector API."""

from dataclasses import FrozenInstanceError

import pytest

try:
    from ge.es import GraphBuilder
    from ge.es.ut_test import Add
    from ge.passes import FuseCheckResult, can_fuse
except ImportError as exc:
    pytest.skip(
        f"Cannot import graph fuse inspector dependencies: {exc}",
        allow_module_level=True,
    )


def _build_linear_add_graph(name):
    builder = GraphBuilder(name)
    input_tensor = builder.create_input(0)
    const1 = builder.create_const_float(1.0)
    const2 = builder.create_const_float(2.0)
    add1 = Add(input_tensor, const1)
    add2 = Add(add1, const2)
    graph = builder.build_and_reset([add2])
    add_nodes = [node for node in graph.get_direct_nodes() if node.type == "Add"]
    assert len(add_nodes) == 2
    return graph, add_nodes


def test_can_fuse_returns_structured_success_for_generator():
    graph, add_nodes = _build_linear_add_graph("can_fuse_success")

    result = can_fuse(node for node in add_nodes)

    assert graph is not None
    assert result == FuseCheckResult(ok=True, reason="")
    with pytest.raises(FrozenInstanceError):
        result.ok = False


def test_can_fuse_returns_reason_for_empty_nodes():
    result = can_fuse([])

    assert result.ok is False
    assert "empty" in result.reason


def test_can_fuse_returns_reason_for_conflicting_stream_labels():
    graph, add_nodes = _build_linear_add_graph("can_fuse_stream_labels")
    add_nodes[0].set_attr("_user_stream_label", "stream_a")
    add_nodes[1].set_attr("_user_stream_label", "stream_b")

    result = can_fuse(add_nodes)

    assert graph is not None
    assert result.ok is False
    assert "stream_a" in result.reason
    assert "stream_b" in result.reason


def test_can_fuse_returns_reason_for_nodes_from_different_graphs():
    graph1, graph1_nodes = _build_linear_add_graph("can_fuse_graph_1")
    graph2, graph2_nodes = _build_linear_add_graph("can_fuse_graph_2")

    result = can_fuse([graph1_nodes[0], graph2_nodes[0]])

    assert graph1 is not None
    assert graph2 is not None
    assert result.ok is False
    assert "different graphs" in result.reason
