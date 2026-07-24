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
    "FuseCheckResult",
    "FusionBasePass",
    "MatchResult",
    "NodeIo",
    "PassStage",
    "PassContext",
    "Pattern",
    "PatternMatcherConfig",
    "PatternMatcherConfigBuilder",
    "PatternFusionPass",
    "SubgraphInput",
    "SubgraphOutput",
    "SubgraphBoundary",
    "SubgraphRewriter",
    "can_fuse",
    "clear_registered_passes",
    "create_pattern",
    "create_replacement",
    "get_registered_pass_by_descriptor_key",
    "get_registered_pass_dicts",
    "get_registered_passes",
    "pattern",
    "register_decompose_pass",
    "register_fusion_pass",
    "report_fuse",
]

from .pattern import pattern

_LAZY_EXPORTS = {
    "DecomposePass": ".base",
    "FuseCheckResult": ".fuse_inspector",
    "FusionBasePass": ".base",
    "MatchResult": ".base",
    "PassStage": ".base",
    "PassContext": ".base",
    "PatternMatcherConfig": ".base",
    "PatternMatcherConfigBuilder": ".base",
    "PatternFusionPass": ".base",
    "SubgraphInput": ".base",
    "SubgraphOutput": ".base",
    "SubgraphBoundary": ".base",
    "SubgraphRewriter": ".base",
    "can_fuse": ".fuse_inspector",
    "NodeIo": ".pattern",
    "Pattern": ".pattern",
    "create_pattern": ".pattern",
    "create_replacement": ".replacement",
    "clear_registered_passes": ".registry",
    "get_registered_pass_by_descriptor_key": ".registry",
    "get_registered_pass_dicts": ".registry",
    "get_registered_passes": ".registry",
    "register_decompose_pass": ".registry",
    "register_fusion_pass": ".registry",
    "report_fuse": ".fuse_inspector",
}


def __getattr__(name: str):
    module_name = _LAZY_EXPORTS.get(name)
    if module_name is None:
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}")

    import importlib

    module = importlib.import_module(module_name, __name__)
    value = getattr(module, name)
    globals()[name] = value
    return value


def __dir__():
    return sorted(set(globals()) | set(__all__))
