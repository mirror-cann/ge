# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

from __future__ import annotations

import typing
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from ge.graph import Graph, Node
    from ge.passes.pattern import NodeIo
    from ge.es.tensor_holder import TensorHolder

__all__: list[str] = [
    "MatchResult",
    "PassContext",
    "Pattern",
    "PatternMatcherConfig",
    "PatternMatcherConfigBuilder",
    "SubgraphBoundary",
    "SubgraphInput",
    "SubgraphOutput",
    "SubgraphRewriter",
]


class PassContext:
    """Compile-time pass context (Python view of C++ ``CustomPassContext``).

    Injected by the engine into ``FusionBasePass.run(graph, context)`` for querying or
    setting pass name, error message, and compilation options.

    **Constraints**

    - Use only inside the current ``run`` call stack (or other engine-defined synchronous
      callbacks); do not store on ``self`` for later threads.
    - ``get_option_value(key)``: raises ``RuntimeError`` if the key is invalid or the
      underlying call does not return ``GRAPH_SUCCESS``.

    **Example**

        def run(self, graph, context: PassContext):
            name = context.get_pass_name()
            opt = context.get_option_value("some.option.key")
            if not self._ok(graph):
                context.set_error_message("my pass: invariant violated")
                return False
            return True

        For more, see https://gitcode.com/cann/ge/tree/master/examples/fusion_pass
    """

    def get_pass_name(self) -> str:
        """Return the current pass name."""
        ...

    def get_error_message(self) -> str:
        """Return the error message set by the engine or pass (empty string if none)."""
        ...

    def set_error_message(self, message: str) -> None:
        """Set an error message for GE to record or surface."""
        ...

    def set_pass_name(self, name: str) -> None:
        """Set the pass name (rarely needed; normally owned by the engine)."""
        ...

    def get_option_value(self, key: str) -> str:
        """Return the string value of a compilation option for ``key``.

        **Constraints**: The key must exist and be readable; otherwise ``RuntimeError``.
        """
        ...


class PatternMatcherConfig:
    """
    Configuration for pattern matcher behaviour (const value match, IR attrs, etc.).
    
    **Example**

        class TestPass(PatternFusionPass):
            def __init__(self):
                super().__init__(
                    PatternMatcherConfigBuilder()
                    .enable_const_value_match()
                    .enable_ir_attr_match()
                    .build()
                )

        For more,see https://gitcode.com/cann/ge/tree/master/examples/fusion_pass
    """

    def is_enable_const_value_match(self) -> bool:
        """Return whether const-value matching is enabled."""
        ...

    def is_enable_ir_attr_match(self) -> bool:
        """Return whether IR attribute matching is enabled."""
        ...


class PatternMatcherConfigBuilder:
    """Fluent builder for ``PatternMatcherConfig``."""

    def __init__(self) -> None:
        """Create an empty builder."""
        ...

    def enable_const_value_match(self) -> PatternMatcherConfigBuilder:
        """Enable const-value matching; returns ``self`` for chaining."""
        ...

    def enable_ir_attr_match(self) -> PatternMatcherConfigBuilder:
        """Enable IR attribute matching; returns ``self`` for chaining."""
        ...

    def build(self) -> PatternMatcherConfig:
        """Build a ``PatternMatcherConfig`` instance.

        **Constraints**: Raises ``RuntimeError`` if the C++ build fails.
        """
        ...


class Pattern:
    """Pattern graph wrapper used to match a subgraph in the target graph.

    **Constraints**

    - ``__init__(graph)`` **moves** the ``graph`` out of the Python wrapper and invalidates
      it; do not use that ``Graph`` variable after construction.
    - ``release()`` hands the native ``Pattern*`` to the C++ pipeline; afterwards
      ``is_valid()`` is ``False`` and methods other than ``is_valid`` / ``release`` raise
      ``RuntimeError``.
    - ``capture_tensor`` accepts a ``TensorHolder`` / ``Node`` plus ``index``, or a
      ``NodeIo``-like object (attributes ``node`` / ``index``) with ``index`` argument
      left as ``None``.

    **Example**

        from ge.passes import create_pattern

        pat = create_pattern(pattern_builder.build_and_reset())
        pat.capture_tensor(matmul)
        native_handle = pat.release()

        For more, see https://gitcode.com/cann/ge/tree/master/examples/fusion_pass

    """

    def __init__(self, graph: Graph) -> None:
        """Construct from a pattern graph (the original ``graph`` handle is invalidated)."""
        ...

    def capture_tensor(self, source: TensorHolder | Node | NodeIo, index: int | None = None) -> Pattern:
        """Record a captured tensor for lookup by index in ``replacement``.

        Returns ``self`` for chaining.
        """
        ...

    def get_captured_tensors(self) -> list[NodeIo]:
        """Return captured tensors as ``NodeIo`` (``node`` + ``index``)."""
        ...

    def is_valid(self) -> bool:
        """Return whether this object still owns an unreleased native ``Pattern``."""
        ...

    def release(self) -> int:
        """Release Python ownership and return the C++ ``Pattern*`` handle as an integer.

        **Constraints**: Successful ``release`` must happen at most once per instance.
        """
        ...


class MatchResult:
    """Result of one pattern match (borrowed C++ ``MatchResult``; lifetime owned by engine/bridge).

    **Constraints**

    - Do not keep ``MatchResult`` beyond the current ``meet_requirements`` / ``replacement`` call.
    - After the bridge calls ``_invalidate()`` in ``finally``, further method calls may fail.
    - ``get_captured_tensor(i)``: ``i`` must follow ``capture_tensor`` order; otherwise the
      C++ layer fails and raises.

    **Example**

        nodes = match_result.get_matched_nodes()
        tensor0 = match_result.get_captured_tensor(0)  # NodeIo
        n0, idx0 = tensor0.node, tensor0.index

        For more, see https://gitcode.com/cann/ge/tree/master/examples/fusion_pass

    """

    def __str__(self) -> str:
        """Human-readable debug string (from C++ ``ToAscendString``)."""
        ...

    def _invalidate(self) -> None:
        """Invalidate this wrapper (GE bridge only; do not call from pass code)."""
        ...

    def get_matched_nodes(self) -> list[Node]:
        """Return matched ``Node`` instances in pattern order."""
        ...

    def get_captured_tensor(self, capture_index: int) -> NodeIo:
        """Return the ``capture_index``-th captured tensor (``NodeIo``) in ``capture_tensor`` order."""
        ...

    def get_pattern_graph_name(self) -> str:
        """Return the pattern graph name."""
        ...


class SubgraphInput:
    """One logical boundary input: a set of main-graph anchors (many-to-one allowed)."""

    @typing.overload
    def __init__(self) -> None:
        """Construct an empty input; use ``add_input`` to append anchors."""
        ...

    @typing.overload
    def __init__(self, node_inputs: typing.Iterable[tuple[Node, int]]) -> None:
        """Construct from an iterable of ``(node, out_index)``.

        **Constraints**: Each item must be a 2-tuple; otherwise the C++ layer raises
        ``RuntimeError``.
        """
        ...

    def add_input(self, node: Node, out_index: int) -> int:
        """Append an input anchor (node output index); return internal index (``uint32`` semantics)."""
        ...


class SubgraphOutput:
    """One logical boundary output: a single main-graph node output anchor."""

    @typing.overload
    def __init__(self) -> None:
        """Construct an empty output; call ``set_output`` next."""
        ...

    @typing.overload
    def __init__(self, node: Node, out_index: int) -> None:
        """Bind one output ``(node, out_index)``."""
        ...

    def set_output(self, node: Node, out_index: int) -> int:
        """Set the output anchor; return internal status (``uint32`` semantics)."""
        ...


class SubgraphBoundary:
    """Input/output boundary of the subgraph to replace in the main graph.

    Indices passed to ``add_input`` / ``add_output`` are **logical** boundary slots and must
    line up with the replacement graph's input/output ordering.
    """

    def __init__(self) -> None:
        """Create an empty boundary."""
        ...

    def add_input(self, index: int, input: SubgraphInput) -> int:
        """Bind the ``index``-th boundary input to ``SubgraphInput``."""
        ...

    def add_output(self, index: int, output: SubgraphOutput) -> int:
        """Bind the ``index``-th boundary output to ``SubgraphOutput``."""
        ...


class SubgraphRewriter:
    """Apply whole-subgraph replacement on the main graph."""

    @staticmethod
    def replace(boundary: SubgraphBoundary, replacement: Graph) -> int:
        """Replace the subgraph described by ``boundary`` with ``replacement``.

        **Constraints**

        - ``replacement`` must be a non-empty graph wrapper; the graph is **copied** on the
          C++ side; you must still follow GE rules for Python ``Graph`` ownership.
        - The return value is the integer form of C++ ``Status``; on failure inspect logs
          for boundary completeness and index alignment.

        **Example**

            b = SubgraphBoundary()
            b.add_input(0, SubgraphInput([(n0, 0), (n0, 1)]))
            b.add_output(0, SubgraphOutput(out_node, 0))
            ret = SubgraphRewriter.replace(b, replacement_graph)

            For more, see https://gitcode.com/cann/ge/tree/master/examples/fusion_pass

        """
        ...
