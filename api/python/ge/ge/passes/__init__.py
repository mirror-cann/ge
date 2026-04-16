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

"""Python GE passes public package."""
__all__ = [
    "DecomposePass",
    "FusionBasePass",
    "MatchResult",
    "PassStage",
    "PatternFusionPass",
    "PassContext",
    "clear_registered_passes",
    "get_registered_pass_by_descriptor_key",
    "get_registered_pass_dicts",
    "get_registered_passes",
    "register_decompose_pass",
    "register_fusion_pass",
]

from .base import DecomposePass
from .base import FusionBasePass
from .base import MatchResult
from .base import PassStage
from .base import PatternFusionPass
from ._ge_pass_native import PassContext
from .registry import clear_registered_passes
from .registry import get_registered_pass_by_descriptor_key
from .registry import get_registered_pass_dicts
from .registry import get_registered_passes
from .registry import register_decompose_pass
from .registry import register_fusion_pass

