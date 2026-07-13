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

"""Bridge-facing Python runtime helpers for GE custom op implementations."""

import sys
import threading
from dataclasses import dataclass
from typing import Dict

from .base import BaseCustomOp, EagerExecuteOp, EagerOpExecutionContext
from .bootstrap import get_registered_op_impls, load_custom_op_plugins
from .registry import get_registered_op_impl_by_descriptor_key


@dataclass
class _OpImplHolder:
    descriptor_key: str
    instance_id: str
    instance: BaseCustomOp


_HOLDER_LOCK = threading.RLock()
_OP_IMPL_HOLDERS: Dict[str, _OpImplHolder] = {}


def load_and_get_op_impl_descriptors() -> list:
    load_custom_op_plugins()
    return get_registered_op_impls()


def _get_holder(instance_id: str) -> _OpImplHolder:
    with _HOLDER_LOCK:
        holder = _OP_IMPL_HOLDERS.get(instance_id)
    if holder is None:
        raise KeyError(f"python op impl holder is not created: {instance_id}")
    return holder


def _get_eager_execute_op(instance_id: str) -> EagerExecuteOp:
    instance = _get_holder(instance_id).instance
    if not isinstance(instance, EagerExecuteOp):
        raise TypeError(
            f"python op impl does not implement EagerExecuteOp: {instance_id}"
        )
    return instance


def create_op_impl_holder(instance_id: str, descriptor_key: str) -> bool:
    descriptor = get_registered_op_impl_by_descriptor_key(descriptor_key)
    if descriptor is None:
        raise KeyError(f"python op impl descriptor_key not found: {descriptor_key}")
    with _HOLDER_LOCK:
        if instance_id in _OP_IMPL_HOLDERS:
            return True
        _OP_IMPL_HOLDERS[instance_id] = _OpImplHolder(
            descriptor_key=descriptor_key,
            instance_id=instance_id,
            instance=descriptor.cls(),
        )
    return True


def destroy_op_impl_holder(instance_id: str) -> bool:
    with _HOLDER_LOCK:
        return _OP_IMPL_HOLDERS.pop(instance_id, None) is not None


def call_execute(instance_id: str, ctx: EagerOpExecutionContext) -> None:
    try:
        custom_op = _get_eager_execute_op(instance_id)
        custom_op.execute(ctx)
    finally:
        ctx._invalidate()


def clear_op_impl_holders() -> None:
    with _HOLDER_LOCK:
        _OP_IMPL_HOLDERS.clear()


def clear_loaded_op_impl_modules() -> None:
    """Clear all dynamically loaded op implementation modules from sys.modules to avoid test pollution."""
    keys_to_remove = [key for key in sys.modules if key.startswith("_ge_py_custom_op_")]
    for key in keys_to_remove:
        del sys.modules[key]
