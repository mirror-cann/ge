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

"""Fusion feasibility checks and reporting helpers for Python GE passes."""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING, Iterable

from . import _native

if TYPE_CHECKING:
    from ge.graph.node import Node


@dataclass(frozen=True)
class FuseCheckResult:
    """Result of checking whether a node set can be safely fused."""

    ok: bool
    reason: str = ""


report_fuse = _native.report_fuse


def can_fuse(nodes: Iterable["Node"]) -> FuseCheckResult:
    """Check whether ``nodes`` can be safely fused into one node."""
    ok, reason = _native.can_fuse(nodes)
    return FuseCheckResult(ok=ok, reason=reason)
