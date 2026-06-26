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

"""Base definitions for Python GE passes."""

from __future__ import annotations

from enum import Enum
from typing import TYPE_CHECKING, Iterable, List, Optional, Union

from ._native import MatchResult, PassContext, PatternMatcherConfig, PatternMatcherConfigBuilder, SubgraphInput, SubgraphOutput, SubgraphBoundary, SubgraphRewriter

if TYPE_CHECKING:
    from ge.graph.graph import Graph
    from ge.graph.node import Node

    from .pattern import Pattern


PatternOrGraph = Union["Pattern", "Graph"]
StatusLike = Optional[Union[bool, int]]


class PassStage(str, Enum):
    """Python-facing pass stage names."""

    BEFORE_INFER_SHAPE = "BeforeInferShape"
    AFTER_INFER_SHAPE = "AfterInferShape"
    AFTER_BUILTIN_FUSION_PASS = "AfterBuiltinFusionPass"
    AFTER_ORIGIN_GRAPH_OPTIMIZE = "AfterOriginGraphOptimize"


class FusionBasePass:
    """Python FusionBasePass contract."""

    def run(self, graph: Graph, context: PassContext) -> StatusLike:
        raise NotImplementedError("FusionBasePass.run must be implemented")


class PatternFusionPass(FusionBasePass):
    """Python PatternFusionPass contract.

    The execution engine calls ``patterns()``, ``meet_requirements()``, and
    ``replacement()`` — **not** ``run()``.  Overriding ``run()`` in a
    ``PatternFusionPass`` subclass has no effect; implement the three hook
    methods above instead.

    Subclasses can use either the legacy graph-building hooks:
      - ``patterns(self) -> Iterable[Pattern | Graph]``
      - ``replacement(self, match_result) -> Graph``

    or expression-style pattern methods:
      - ``@pattern def name(self, inputs) -> TensorHolder | list[TensorHolder] | tuple[TensorHolder, ...]``
      - ``replacement(self, inputs) -> TensorHolder | list[TensorHolder] | tuple[TensorHolder, ...] | Graph``
      - ``replacement(self, inputs, match_result)`` when match details are needed

    In a ``@pattern`` method, a list/tuple means multiple outputs of one
    pattern, not multiple patterns. Declare several ``@pattern`` methods for
    a multiple-pattern pass. Expression-style patterns automatically capture
    accessed inputs in input-index order, then returned outputs in return order.
    """

    def __init__(self, matcher_config: Optional[PatternMatcherConfig] = None) -> None:
        self._matcher_config = matcher_config

    def __init_subclass__(cls, **kwargs) -> None:
        super().__init_subclass__(**kwargs)
        if "run" in cls.__dict__:
            raise TypeError(
                f"{cls.__name__} overrides run(), which is never invoked "
                f"by the PatternFusionPass execution path. "
                f"Implement patterns()/replacement() instead."
            )
        from .pattern import (
            _adapt_decorated_pattern_methods,
            _adapt_expression_replacement,
            _has_decorated_pattern_methods,
        )

        if _has_decorated_pattern_methods(cls):
            if "patterns" in cls.__dict__:
                raise TypeError(
                    f"{cls.__name__} cannot combine @pattern methods with patterns(). "
                    f"Use one style for declaring patterns."
                )
            cls.patterns = _adapt_decorated_pattern_methods(cls)
        if "replacement" in cls.__dict__:
            cls.replacement = _adapt_expression_replacement(cls.__dict__["replacement"])

    @property
    def matcher_config(self) -> Optional[PatternMatcherConfig]:
        return self._matcher_config

    def patterns(self) -> Iterable[PatternOrGraph]:
        raise NotImplementedError("PatternFusionPass.patterns must be implemented")

    def meet_requirements(self, match_result: MatchResult) -> bool:
        return True

    def replacement(self, match_result: MatchResult) -> "Graph":
        raise NotImplementedError("PatternFusionPass.replacement must be implemented")


class DecomposePass(FusionBasePass):
    """Python DecomposePass contract.

    The execution engine calls ``meet_requirements()`` and ``replacement()``
    for matched nodes — **not** ``run()``. Overriding ``run()`` in a
    ``DecomposePass`` subclass has no effect; implement the two hook methods
    above instead.
    """

    op_types: Optional[List[str]] = None

    def __init_subclass__(cls, **kwargs) -> None:
        super().__init_subclass__(**kwargs)
        if "run" in cls.__dict__:
            raise TypeError(
                f"{cls.__name__} overrides run(), which is never invoked "
                f"by the DecomposePass execution path. "
                f"Implement meet_requirements()/replacement() instead."
            )

    def meet_requirements(self, node: "Node") -> bool:
        return True

    def replacement(self, node: "Node") -> "Graph":
        raise NotImplementedError("DecomposePass.replacement must be implemented")
