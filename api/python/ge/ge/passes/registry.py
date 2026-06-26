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

"""Python pass registry and decorators."""

import inspect
import threading
from collections.abc import Iterable as IterableABC
from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Optional, Type

from .base import DecomposePass, FusionBasePass, PassStage, PatternFusionPass

PASS_KIND_FUSION_BASE = "fusion_base"
PASS_KIND_PATTERN_FUSION = "pattern_fusion"
PASS_KIND_DECOMPOSE = "decompose"


@dataclass(frozen=True)
class PassDescriptor:
    """Normalized Python pass descriptor."""

    descriptor_key: str
    pass_name: str
    module_name: str
    class_name: str
    stage: PassStage
    kind: str
    cls: Type[FusionBasePass]
    op_types: List[str] = field(default_factory=list)

    def to_bridge_dict(self) -> dict:
        return {
            "descriptor_key": self.descriptor_key,
            "pass_name": self.pass_name,
            "module_name": self.module_name,
            "class_name": self.class_name,
            "stage": self.stage.value,
            "kind": self.kind,
            "op_types": list(self.op_types),
        }


class _PassRegistry:
    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._descriptor_key_to_desc: Dict[str, PassDescriptor] = {}
        self._pass_name_to_desc: Dict[str, PassDescriptor] = {}

    def clear(self) -> None:
        with self._lock:
            self._descriptor_key_to_desc.clear()
            self._pass_name_to_desc.clear()

    def register(self, descriptor: PassDescriptor) -> PassDescriptor:
        with self._lock:
            if descriptor.descriptor_key in self._descriptor_key_to_desc:
                raise ValueError(f"python pass descriptor_key already exists: {descriptor.descriptor_key}")
            if descriptor.pass_name in self._pass_name_to_desc:
                raise ValueError(f"python pass name already exists: {descriptor.pass_name}")
            self._descriptor_key_to_desc[descriptor.descriptor_key] = descriptor
            self._pass_name_to_desc[descriptor.pass_name] = descriptor
        return descriptor

    def get_by_descriptor_key(self, descriptor_key: str) -> Optional[PassDescriptor]:
        with self._lock:
            return self._descriptor_key_to_desc.get(descriptor_key)

    def get_all(self) -> List[PassDescriptor]:
        with self._lock:
            return list(self._descriptor_key_to_desc.values())


_PASS_REGISTRY = _PassRegistry()


def _build_descriptor_key(module_name: str, class_name: str, pass_name: str) -> str:
    return f"{module_name}:{class_name}:{pass_name}"


def _normalize_decompose_op_types(op_types: Iterable[str]) -> List[str]:
    if isinstance(op_types, (str, bytes)) or not isinstance(op_types, IterableABC):
        raise TypeError("register_decompose_pass op_types must be an iterable of strings")

    normalized_op_types = list(op_types)
    if not normalized_op_types:
        raise ValueError("register_decompose_pass requires at least one op type")

    for op_type in normalized_op_types:
        if not isinstance(op_type, str) or not op_type:
            raise TypeError("register_decompose_pass op_types must contain non-empty strings")
    return normalized_op_types


def _register_pass_class(
    cls: Type[FusionBasePass],
    *,
    kind: str,
    name: str,
    stage: PassStage,
    op_types: Optional[Iterable[str]] = None,
) -> Type[FusionBasePass]:
    module_name = cls.__module__
    class_name = cls.__name__
    descriptor = PassDescriptor(
        descriptor_key=_build_descriptor_key(module_name, class_name, name),
        pass_name=name,
        module_name=module_name,
        class_name=class_name,
        stage=stage,
        kind=kind,
        cls=cls,
        op_types=list(op_types or []),
    )
    _PASS_REGISTRY.register(descriptor)
    setattr(cls, "__ge_pass_descriptor__", descriptor)
    return cls


def register_fusion_pass(*, name: str, stage: PassStage, kind: Optional[str] = None) -> callable:
    """Decorator for FusionBasePass and PatternFusionPass."""

    def decorator(cls: Type[FusionBasePass]) -> Type[FusionBasePass]:
        if not inspect.isclass(cls) or not issubclass(cls, FusionBasePass):
            raise TypeError("register_fusion_pass expects a FusionBasePass subclass")
        pass_kind = kind
        if pass_kind is None:
            pass_kind = PASS_KIND_PATTERN_FUSION if issubclass(cls, PatternFusionPass) else PASS_KIND_FUSION_BASE
        return _register_pass_class(cls, kind=pass_kind, name=name, stage=stage)

    return decorator


def register_decompose_pass(*, name: str, stage: PassStage, op_types: Iterable[str]) -> callable:
    """Decorator for DecomposePass."""

    normalized_op_types = _normalize_decompose_op_types(op_types)

    def decorator(cls: Type[DecomposePass]) -> Type[DecomposePass]:
        if not inspect.isclass(cls) or not issubclass(cls, DecomposePass):
            raise TypeError("register_decompose_pass expects a DecomposePass subclass")
        setattr(cls, "op_types", list(normalized_op_types))
        return _register_pass_class(
            cls,
            kind=PASS_KIND_DECOMPOSE,
            name=name,
            stage=stage,
            op_types=normalized_op_types,
        )

    return decorator


def clear_registered_passes() -> None:
    _PASS_REGISTRY.clear()


def get_registered_passes() -> List[PassDescriptor]:
    return _PASS_REGISTRY.get_all()


def get_registered_pass_dicts() -> List[dict]:
    return [item.to_bridge_dict() for item in get_registered_passes()]


def get_registered_pass_by_descriptor_key(
    descriptor_key: str,
) -> Optional[PassDescriptor]:
    return _PASS_REGISTRY.get_by_descriptor_key(descriptor_key)
