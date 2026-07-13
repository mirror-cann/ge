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

"""Python GE custom op public package."""

__all__ = [
    "BaseCustomOp",
    "EagerExecuteOp",
    "EagerOpExecutionContext",
    "clear_registered_op_impls",
    "get_registered_op_impl_by_descriptor_key",
    "get_registered_op_impl_dicts",
    "get_registered_op_impls",
    "register_op_impl",
]

_LAZY_EXPORTS = {
    "BaseCustomOp": ".base",
    "EagerExecuteOp": ".base",
    "EagerOpExecutionContext": ".base",
    "clear_registered_op_impls": ".registry",
    "get_registered_op_impl_by_descriptor_key": ".registry",
    "get_registered_op_impl_dicts": ".registry",
    "get_registered_op_impls": ".registry",
    "register_op_impl": ".registry",
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
