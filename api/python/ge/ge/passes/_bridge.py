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

"""Bridge-facing Python runtime helpers for GE passes."""

import sys
import threading
from collections.abc import Iterable
from dataclasses import dataclass
from typing import Dict, Optional, cast

from ge.graph import Graph
from ge.graph import Node

from .base import DecomposePass, FusionBasePass, PassContext, PatternFusionPass, PatternMatcherConfig, StatusLike
from .bootstrap import get_registered_passes, load_pass_plugins
from .pattern import ensure_pattern
from .registry import get_registered_pass_by_descriptor_key
from ._native import borrow_node
from ._native import borrow_match_result
from ._native import clone_pattern_matcher_config
from ._native import release_graph


@dataclass
class _PassHolder:
    descriptor_key: str
    instance_id: str
    instance: FusionBasePass


_HOLDER_LOCK = threading.RLock()
_PASS_HOLDERS: Dict[str, _PassHolder] = {}


def load_and_get_pass_descriptors() -> list:
    load_pass_plugins()
    return get_registered_passes()


def _get_holder(instance_id: str) -> _PassHolder:
    with _HOLDER_LOCK:
        holder = _PASS_HOLDERS.get(instance_id)
    if holder is None:
        raise KeyError(f"python pass holder is not created: {instance_id}")
    return holder


def _get_fusion_base_pass(instance_id: str) -> FusionBasePass:
    return _get_holder(instance_id).instance


def _get_pattern_fusion_pass(instance_id: str) -> PatternFusionPass:
    instance = _get_holder(instance_id).instance
    if not isinstance(instance, PatternFusionPass):
        raise TypeError(f"python pass holder is not PatternFusionPass: {instance_id}")
    return instance


def _get_decompose_pass(instance_id: str) -> DecomposePass:
    instance = _get_holder(instance_id).instance
    if not isinstance(instance, DecomposePass):
        raise TypeError(f"python pass holder is not DecomposePass: {instance_id}")
    return instance


def create_pass_holder(instance_id: str, descriptor_key: str) -> bool:
    descriptor = get_registered_pass_by_descriptor_key(descriptor_key)
    if descriptor is None:
        raise KeyError(f"python pass descriptor_key not found: {descriptor_key}")
    with _HOLDER_LOCK:
        if instance_id in _PASS_HOLDERS:
            return True
        _PASS_HOLDERS[instance_id] = _PassHolder(
            descriptor_key=descriptor_key,
            instance_id=instance_id,
            instance=descriptor.cls(),
        )
    return True


def destroy_pass_holder(instance_id: str) -> bool:
    with _HOLDER_LOCK:
        return _PASS_HOLDERS.pop(instance_id, None) is not None


def run_fusion_base_pass(instance_id: str, graph: Graph, context: Optional[PassContext] = None) -> StatusLike:
    pass_instance = _get_fusion_base_pass(instance_id)
    if context is not None and not isinstance(context, PassContext):
        raise TypeError("context type error")
    return pass_instance.run(graph, cast(PassContext, context))


def _release_replacement_graph(replacement: Graph, pass_name: str) -> int:
    if not isinstance(replacement, Graph):
        raise TypeError(f"{pass_name}.replacement must return ge.graph.Graph")
    return release_graph(replacement)


def get_pass_patterns(instance_id: str) -> list[int]:
    pass_instance = _get_pattern_fusion_pass(instance_id)
    patterns = pass_instance.patterns()
    if patterns is None:
        return []
    if not isinstance(patterns, Iterable) or isinstance(patterns, (str, bytes)):
        raise TypeError("PatternFusionPass.patterns must return an iterable of Pattern or Graph")

    released_patterns = []
    for item in patterns:
        pattern = ensure_pattern(item)
        released_patterns.append(pattern.release())
    return released_patterns


def get_pattern_matcher_config(instance_id: str) -> Optional[int]:
    pass_instance = _get_pattern_fusion_pass(instance_id)
    matcher_config = pass_instance.matcher_config
    if matcher_config is None:
        return None
    if not isinstance(matcher_config, PatternMatcherConfig):
        raise TypeError("PatternFusionPass.matcher_config must be PatternMatcherConfig or None")
    return clone_pattern_matcher_config(matcher_config)


def call_meet_requirements(instance_id: str, match_result_handle: int) -> bool:
    pass_instance = _get_pattern_fusion_pass(instance_id)
    match_result = borrow_match_result(match_result_handle)
    try:
        return bool(pass_instance.meet_requirements(match_result))
    finally:
        match_result._invalidate()


def call_replacement(instance_id: str, match_result_handle: int) -> int:
    pass_instance = _get_pattern_fusion_pass(instance_id)
    match_result = borrow_match_result(match_result_handle)
    try:
        replacement = pass_instance.replacement(match_result)
    finally:
        match_result._invalidate()

    return _release_replacement_graph(replacement, "PatternFusionPass")


def call_decompose_meet_requirements(instance_id: str, node_handle: int) -> bool:
    pass_instance = _get_decompose_pass(instance_id)
    node = cast(Node, borrow_node(node_handle))
    return bool(pass_instance.meet_requirements(node))


def call_decompose_replacement(instance_id: str, node_handle: int) -> int:
    pass_instance = _get_decompose_pass(instance_id)
    node = cast(Node, borrow_node(node_handle))
    replacement = pass_instance.replacement(node)
    return _release_replacement_graph(replacement, "DecomposePass")


def clear_pass_holders() -> None:
    with _HOLDER_LOCK:
        _PASS_HOLDERS.clear()


def clear_loaded_pass_modules() -> None:
    """Clear all dynamically loaded pass modules from sys.modules to avoid test pollution."""
    keys_to_remove = [key for key in sys.modules if key.startswith("_ge_py_pass_")]
    for key in keys_to_remove:
        del sys.modules[key]
