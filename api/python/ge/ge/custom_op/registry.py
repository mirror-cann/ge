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

"""Python custom op implementation registry and decorators."""

import inspect
import threading
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Type

from .base import BaseCustomOp, EagerExecuteOp

INTERFACE_EAGER_EXECUTE = "eager_execute"
_INTERFACE_SPECS = ((INTERFACE_EAGER_EXECUTE, EagerExecuteOp),)


@dataclass(frozen=True)
class OpImplDescriptor:
    """Normalized Python custom op implementation descriptor."""

    descriptor_key: str
    op_type: str
    module_name: str
    class_name: str
    interfaces: List[str] = field(default_factory=list)
    cls: Type[BaseCustomOp] = field(compare=False, repr=False, default=BaseCustomOp)

    def to_bridge_dict(self) -> dict:
        return {
            "descriptor_key": self.descriptor_key,
            "op_type": self.op_type,
            "module_name": self.module_name,
            "class_name": self.class_name,
            "interfaces": list(self.interfaces),
        }


class _OpImplRegistry:
    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._descriptor_key_to_desc: Dict[str, OpImplDescriptor] = {}
        self._op_type_to_desc: Dict[str, OpImplDescriptor] = {}

    def clear(self) -> None:
        with self._lock:
            self._descriptor_key_to_desc.clear()
            self._op_type_to_desc.clear()

    def register(self, descriptor: OpImplDescriptor) -> OpImplDescriptor:
        with self._lock:
            if descriptor.descriptor_key in self._descriptor_key_to_desc:
                raise ValueError(
                    f"python op impl descriptor_key already exists: {descriptor.descriptor_key}"
                )
            if descriptor.op_type in self._op_type_to_desc:
                raise ValueError(
                    f"python op impl type already exists: {descriptor.op_type}"
                )
            self._descriptor_key_to_desc[descriptor.descriptor_key] = descriptor
            self._op_type_to_desc[descriptor.op_type] = descriptor
        return descriptor

    def get_by_descriptor_key(self, descriptor_key: str) -> Optional[OpImplDescriptor]:
        with self._lock:
            return self._descriptor_key_to_desc.get(descriptor_key)

    def get_all(self) -> List[OpImplDescriptor]:
        with self._lock:
            return list(self._descriptor_key_to_desc.values())


_OP_IMPL_REGISTRY = _OpImplRegistry()


def _build_descriptor_key(module_name: str, class_name: str, op_type: str) -> str:
    return f"{module_name}:{class_name}:{op_type}"


def _normalize_op_type(op_type: str) -> str:
    if not isinstance(op_type, str) or not op_type:
        raise TypeError("register_op_impl op_type must be a non-empty string")
    return op_type


def _collect_interfaces(cls: Type[BaseCustomOp]) -> List[str]:
    return [name for name, base_cls in _INTERFACE_SPECS if issubclass(cls, base_cls)]


def _get_interfaces(cls: Type[BaseCustomOp]) -> List[str]:
    interfaces = _collect_interfaces(cls)
    if not interfaces:
        raise TypeError("register_op_impl expects a supported BaseCustomOp subclass")
    return interfaces


def _register_op_impl_class(
    cls: Type[BaseCustomOp], *, op_type: str
) -> Type[BaseCustomOp]:
    module_name = cls.__module__
    class_name = cls.__name__
    descriptor = OpImplDescriptor(
        descriptor_key=_build_descriptor_key(module_name, class_name, op_type),
        op_type=op_type,
        module_name=module_name,
        class_name=class_name,
        interfaces=_get_interfaces(cls),
        cls=cls,
    )
    _OP_IMPL_REGISTRY.register(descriptor)
    setattr(cls, "__ge_op_impl_descriptor__", descriptor)
    return cls


def register_op_impl(*, op_type: str) -> callable:
    """Decorator for Python custom op implementation classes."""

    normalized_op_type = _normalize_op_type(op_type)

    def decorator(cls: Type[BaseCustomOp]) -> Type[BaseCustomOp]:
        if not inspect.isclass(cls) or not issubclass(cls, BaseCustomOp):
            raise TypeError("register_op_impl expects a BaseCustomOp subclass")
        return _register_op_impl_class(cls, op_type=normalized_op_type)

    return decorator


def clear_registered_op_impls() -> None:
    _OP_IMPL_REGISTRY.clear()


def get_registered_op_impls() -> List[OpImplDescriptor]:
    return _OP_IMPL_REGISTRY.get_all()


def get_registered_op_impl_dicts() -> List[dict]:
    return [item.to_bridge_dict() for item in get_registered_op_impls()]


def get_registered_op_impl_by_descriptor_key(
    descriptor_key: str,
) -> Optional[OpImplDescriptor]:
    return _OP_IMPL_REGISTRY.get_by_descriptor_key(descriptor_key)
