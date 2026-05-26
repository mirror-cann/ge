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
    "clone_pattern_matcher_config",
    "release_graph",
]

import sys
from ._artifact_utils import current_platform_tag
from ._artifact_utils import current_python_tag
from ._artifact_utils import find_prebuilt_artifact
from ._artifact_utils import artifacts_root
from ._artifact_utils import BRIDGE_ABI_VERSION
from ._artifact_utils import iter_artifacts
from ._artifact_utils import load_native_module
from ._artifact_utils import NATIVE_MODULE_NAME


def _format_missing_artifact_error() -> str:
    python_tag = current_python_tag()
    platform_tag = current_platform_tag()
    discovered_artifacts = sorted(
        f"{artifact.python_tag}-{artifact.platform_tag}-abi{artifact.bridge_abi}"
        for artifact in iter_artifacts()
    )
    discovered_text = ", ".join(discovered_artifacts) if discovered_artifacts else "none"
    expected_wheel = f"ge_py_pass_bridge-*-{python_tag}-{python_tag}-*.whl"
    return (
        "No matching GE Python pass native artifact was found for runtime "
        f"python tag '{python_tag}', platform '{platform_tag}', "
        f"bridge ABI {BRIDGE_ABI_VERSION}. "
        f"Searched artifact root: {artifacts_root()}. "
        f"Discovered valid artifacts: {discovered_text}. "
        "Please install the native artifact wheel that matches this Python "
        f"runtime, for example '{expected_wheel}', or reinstall the CANN run "
        "package that contains the matching ge_py_pass_bridge wheel."
    )


_native = sys.modules.get(NATIVE_MODULE_NAME)
if _native is None:
    _artifact = find_prebuilt_artifact()
    if _artifact is None:
        raise ImportError(_format_missing_artifact_error())
    _native = load_native_module(_artifact.native_path)

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
clone_pattern_matcher_config = _native.clone_pattern_matcher_config
release_graph = _native.release_graph
