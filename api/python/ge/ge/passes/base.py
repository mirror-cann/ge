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

"""Base definitions for Python GE passes."""

from dataclasses import dataclass, field
from enum import Enum
from typing import Any, List, Optional


class PassStage(str, Enum):
    """Python-facing pass stage names."""

    BEFORE_INFER_SHAPE = "BeforeInferShape"
    AFTER_INFER_SHAPE = "AfterInferShape"
    AFTER_ASSIGN_LOGIC_STREAM = "AfterAssignLogicStream"
    AFTER_BUILTIN_FUSION_PASS = "AfterBuiltinFusionPass"
    AFTER_ORIGIN_GRAPH_OPTIMIZE = "AfterOriginGraphOptimize"


@dataclass(frozen=True)
class PassContext:
    """Lightweight Python-side pass context view."""

    pass_name: str = ""
    options: dict = field(default_factory=dict)


@dataclass(frozen=True)
class MatchResult:
    """Placeholder for future native MatchResult binding."""

    captured_nodes: dict = field(default_factory=dict)
    captured_tensors: dict = field(default_factory=dict)


class FusionBasePass:
    """Python FusionBasePass contract."""

    def run(self, graph: Any, context: PassContext) -> Any:
        raise NotImplementedError("FusionBasePass.run must be implemented")

    def Run(self, graph: Any, context: PassContext) -> Any:  # noqa: N802
        return self.run(graph, context)


class PatternFusionPass(FusionBasePass):
    """Python PatternFusionPass contract."""

    def patterns(self) -> List[Any]:
        raise NotImplementedError("PatternFusionPass.patterns must be implemented")

    def meet_requirements(self, match_result: MatchResult) -> bool:
        return True

    def replacement(self, match_result: MatchResult) -> Any:
        raise NotImplementedError("PatternFusionPass.replacement must be implemented")


class DecomposePass(FusionBasePass):
    """Python DecomposePass contract."""

    op_types: Optional[List[str]] = None

    def meet_requirements(self, node: Any) -> bool:
        return True

    def replacement(self, node: Any) -> Any:
        raise NotImplementedError("DecomposePass.replacement must be implemented")
