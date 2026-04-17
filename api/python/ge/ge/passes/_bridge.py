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
from dataclasses import dataclass
from typing import Any, Dict, Optional

from .bootstrap import get_registered_passes, load_pass_plugins
from .registry import get_registered_pass_by_descriptor_key
from ._ge_pass_native import PassContext


@dataclass
class _PassHolder:
    descriptor_key: str
    instance_id: str
    instance: Any


_HOLDER_LOCK = threading.RLock()
_PASS_HOLDERS: Dict[str, _PassHolder] = {}


def load_and_get_pass_descriptors() -> list:
    load_pass_plugins()
    return get_registered_passes()


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


def run_fusion_base_pass(instance_id: str, graph: Any, context: Optional[Any] = None) -> Any:
    with _HOLDER_LOCK:
        holder = _PASS_HOLDERS.get(instance_id)
    if holder is None:
        raise KeyError(f"python pass holder is not created: {instance_id}")
    if not isinstance(context, PassContext):
        raise KeyError(f"context type error")
    return holder.instance.run(graph, context)


def clear_pass_holders() -> None:
    with _HOLDER_LOCK:
        _PASS_HOLDERS.clear()


def clear_loaded_pass_modules() -> None:
    """Clear all dynamically loaded pass modules from sys.modules to avoid test pollution."""
    keys_to_remove = [key for key in sys.modules if key.startswith("_ge_py_pass_")]
    for key in keys_to_remove:
        del sys.modules[key]
