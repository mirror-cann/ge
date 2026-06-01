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
from functools import wraps
import inspect
from typing import Callable, Dict, Iterator, List, Type, Union, overload

from ge.graph import Graph, Node
from ge.es.graph_builder import GraphBuilder
from ge.es.tensor_holder import TensorHolder

from ._native import Pattern


_PATTERN_METHOD_MARK = "__ge_expression_pattern_method__"


@dataclass(frozen=True)
class NodeIo:
    """Python helper describing one node output."""

    node: Node
    index: int = 0


class PatternInputs:
    """Lazy graph input collection passed to expression-style pass hooks.

    Users receive this object from ``@pattern`` methods or
    ``replacement(self, inputs)``. Accessing ``inputs[i]`` creates graph input
    ``i`` on demand. Use ``inputs[:N]`` to declare multiple inputs explicitly.
    The input count is intentionally unknown, so direct iteration is rejected.
    """

    def __init__(self, builder: GraphBuilder) -> None:
        self._builder = builder
        self._inputs: Dict[int, TensorHolder] = {}

    @overload
    def __getitem__(self, index: int) -> TensorHolder:
        ...

    @overload
    def __getitem__(self, index: slice) -> List[TensorHolder]:
        ...

    def __getitem__(self, index: Union[int, slice]) -> Union[TensorHolder, List[TensorHolder]]:
        if isinstance(index, slice):
            return self._get_slice(index)
        if not isinstance(index, int):
            raise TypeError("PatternInputs index must be an integer or slice")
        if index < 0:
            raise ValueError("PatternInputs index must be non-negative")
        self._ensure_created(index)
        return self._inputs[index]

    def __iter__(self) -> Iterator[TensorHolder]:
        raise TypeError("PatternInputs cannot be iterated because the input count is unknown; use inputs[:N]")

    def _get_slice(self, index_slice: slice) -> List[TensorHolder]:
        if index_slice.step not in (None, 1):
            raise ValueError("PatternInputs slice step must be 1")
        start = 0 if index_slice.start is None else index_slice.start
        stop = index_slice.stop
        if stop is None:
            raise ValueError("PatternInputs slice must provide a stop index, for example inputs[:3]")
        if start < 0 or stop < 0:
            raise ValueError("PatternInputs slice indices must be non-negative")
        if stop < start:
            raise ValueError("PatternInputs slice stop must be greater than or equal to start")
        return [self[i] for i in range(start, stop)]

    def _ensure_created(self, index: int) -> None:
        for input_index in range(len(self._inputs), index + 1):
            self._inputs[input_index] = self._builder.create_input(input_index)

    def created(self) -> List[TensorHolder]:
        """Return created inputs in graph input index order."""

        return [self._inputs[index] for index in sorted(self._inputs)]


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


def pattern(method: Callable[..., object]) -> Callable[..., object]:
    """Mark a ``PatternFusionPass`` method as one expression-style pattern."""

    setattr(method, _PATTERN_METHOD_MARK, True)
    return method


def _has_decorated_pattern_methods(cls: Type[object]) -> bool:
    return bool(_get_decorated_pattern_methods(cls))


def _adapt_decorated_pattern_methods(cls: Type[object]) -> Callable[..., object]:
    """Build legacy ``patterns(self)`` from ``@pattern`` expression methods."""

    methods = _get_decorated_pattern_methods(cls)
    for method in methods:
        _check_required_arg_count(method, min_count=2, max_count=2,
                                  message="@pattern methods only support method(self, inputs)")

    def wrapper(self) -> List[Pattern]:
        patterns = []
        for method in methods:
            builder = GraphBuilder(_decorated_pattern_graph_name(self, method))
            inputs = PatternInputs(builder)
            result = method(self, inputs)
            patterns.extend(_build_patterns_from_expression_result(builder, inputs, result))
        return patterns

    return wrapper


def _adapt_expression_replacement(method: Callable[..., object]) -> Callable[..., object]:
    """Wrap ``replacement(self, inputs[, match_result])`` into the legacy hook."""

    positional_params = _positional_params(method)
    if len(positional_params) < 2 or positional_params[1].name != "inputs":
        return method
    _check_required_arg_count(
        method,
        min_count=2,
        max_count=3,
        message="Expression-style replacement only supports "
                "replacement(self, inputs) or replacement(self, inputs, match_result)",
    )
    accepts_match_result = _positional_arg_count(method) == 3

    @wraps(method)
    def wrapper(self, match_result: object) -> Graph:
        builder = GraphBuilder(_default_graph_name(self, "replacement"))
        inputs = PatternInputs(builder)
        if accepts_match_result:
            result = method(self, inputs, match_result)
        else:
            result = method(self, inputs)
        return _build_replacement_from_expression_result(builder, result)

    return wrapper


def _check_required_arg_count(method: Callable[..., object], *, min_count: int,
                              max_count: int, message: str) -> None:
    count = _positional_arg_count(method)
    if count < min_count or count > max_count or _has_required_keyword_only_args(method):
        raise TypeError(message)


def _positional_arg_count(method: Callable[..., object]) -> int:
    return len(_positional_params(method))


def _positional_params(method: Callable[..., object]) -> List[inspect.Parameter]:
    signature = inspect.signature(method)
    return [
        param for param in signature.parameters.values()
        if param.kind in (inspect.Parameter.POSITIONAL_ONLY, inspect.Parameter.POSITIONAL_OR_KEYWORD)
    ]


def _has_required_keyword_only_args(method: Callable[..., object]) -> bool:
    signature = inspect.signature(method)
    return any(
        param.kind == inspect.Parameter.KEYWORD_ONLY and param.default is inspect.Parameter.empty
        for param in signature.parameters.values()
    )


def _get_decorated_pattern_methods(cls: Type[object]) -> List[Callable[..., object]]:
    return [
        method for method in cls.__dict__.values()
        if callable(method) and getattr(method, _PATTERN_METHOD_MARK, False)
    ]


def _default_graph_name(instance: object, suffix: str) -> str:
    return f"{instance.__class__.__name__}_{suffix}"


def _decorated_pattern_graph_name(instance: object, method: Callable[..., object]) -> str:
    return f"{instance.__class__.__name__}_{method.__name__}_pattern"


def _build_patterns_from_expression_result(builder: GraphBuilder, inputs: PatternInputs,
                                           result: object) -> List[Pattern]:
    if _is_pattern_or_graph(result):
        return [ensure_pattern(result)]
    if isinstance(result, (list, tuple)) and result and all(_is_pattern_or_graph(item) for item in result):
        raise TypeError(
            "A @pattern method supports a single pattern only. "
            "For multiple patterns, declare several @pattern methods or use legacy patterns(self)."
        )

    pattern_outputs = _normalize_tensor_outputs(result, "patterns")
    graph = builder.build_and_reset(pattern_outputs)
    built_pattern = create_pattern(graph)
    for input_tensor in inputs.created():
        built_pattern.capture_tensor(input_tensor)
    for output_tensor in pattern_outputs:
        built_pattern.capture_tensor(output_tensor)
    return [built_pattern]


def _build_replacement_from_expression_result(builder: GraphBuilder, result: object) -> Graph:
    if isinstance(result, Graph):
        return result
    return builder.build_and_reset(_normalize_tensor_outputs(result, "replacement"))


def _normalize_tensor_outputs(result: object, hook_name: str) -> List[TensorHolder]:
    if isinstance(result, TensorHolder):
        return [result]
    if isinstance(result, tuple):
        result = list(result)
    if isinstance(result, list) and result and all(isinstance(item, TensorHolder) for item in result):
        return result
    raise TypeError(
        f"Expression-style {hook_name} must return a TensorHolder or a non-empty list/tuple of TensorHolder"
    )


def _is_pattern_or_graph(result: object) -> bool:
    return isinstance(result, (Pattern, Graph))
