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

"""Load and re-export the selected Python pass native module."""

from __future__ import annotations

__all__ = [
    "MatchResult",
    "PassContext",
    "Pattern",
    "PatternMatcherConfig",
    "PatternMatcherConfigBuilder",
    "SubgraphInput",
    "SubgraphOutput",
    "SubgraphBoundary",
    "SubgraphRewriter",
    "borrow_match_result",
    "borrow_node",
    "can_fuse",
    "clone_pattern_matcher_config",
    "report_fuse",
    "release_graph",
]

from .runtime import ensure_native_module

_native = ensure_native_module()

MatchResult = _native.MatchResult
PassContext = _native.PassContext
Pattern = _native.Pattern
PatternMatcherConfig = _native.PatternMatcherConfig
PatternMatcherConfigBuilder = _native.PatternMatcherConfigBuilder
SubgraphInput = _native.SubgraphInput
SubgraphOutput = _native.SubgraphOutput
SubgraphBoundary = _native.SubgraphBoundary
SubgraphRewriter = _native.SubgraphRewriter
borrow_match_result = _native.borrow_match_result
borrow_node = _native.borrow_node
can_fuse = _native.can_fuse
clone_pattern_matcher_config = _native.clone_pattern_matcher_config
report_fuse = _native.report_fuse
release_graph = _native.release_graph
