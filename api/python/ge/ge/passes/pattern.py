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

"""PatternFusionPass helper types and constructors."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Union

from ge.graph import Graph, Node
from ge.es.tensor_holder import TensorHolder

from ._native import Pattern


@dataclass(frozen=True)
class NodeIo:
    """Python helper describing one node output."""

    node: Node
    index: int = 0


def capture_tensor(source: Union[NodeIo, Node, TensorHolder], index: int = 0) -> NodeIo:
    """Normalize a captured tensor source into a NodeIo helper."""

    if isinstance(source, NodeIo):
        return source
    if isinstance(source, TensorHolder):
        return NodeIo(source._get_node_snapshot(), index)
    if isinstance(source, Node):
        return NodeIo(source, index)
    raise TypeError("capture_tensor expects NodeIo, Node, or TensorHolder")


def create_pattern(graph: Graph) -> Pattern:
    """Build a native Pattern from a pattern graph."""

    return Pattern(graph)


def ensure_pattern(pattern_or_graph: Union[Pattern, Graph]) -> Pattern:
    """Convert a Graph into Pattern on demand for bridge helpers."""

    if isinstance(pattern_or_graph, Graph):
        return create_pattern(pattern_or_graph)
    if isinstance(pattern_or_graph, Pattern):
        return pattern_or_graph
    raise TypeError("PatternFusionPass.patterns must return Pattern or Graph objects")
