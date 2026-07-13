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

"""Load and re-export the Python custom op native module."""

from __future__ import annotations

__all__ = [
    "EagerOpExecutionContext",
]

from importlib import import_module

from ge.runtime import _native as _runtime_native  # noqa: F401

from ._artifact_utils import find_prebuilt_artifact, load_native_module


def _load_native_module():
    artifact = find_prebuilt_artifact()
    if artifact is not None:
        return load_native_module(artifact.native_path)
    return import_module("ge.custom_op._ge_custom_op_native")


_native = _load_native_module()

EagerOpExecutionContext = _native.EagerOpExecutionContext
