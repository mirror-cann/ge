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

"""Base definitions for Python GE custom ops."""

from __future__ import annotations
import inspect
from abc import ABC, abstractmethod
from functools import wraps
from typing import Callable, List

from ._native import EagerOpExecutionContext


_MISSING = object()
_POSITIONAL_PARAMETER_KINDS = (
    inspect.Parameter.POSITIONAL_ONLY,
    inspect.Parameter.POSITIONAL_OR_KEYWORD,
)
_EXECUTE_CONTEXT_ARG_NAME = "ctx"
_EXECUTE_INPUTS_ARG_NAME = "inputs"
_SUPPORTED_EXECUTE_ARG_NAMES = (_EXECUTE_CONTEXT_ARG_NAME, _EXECUTE_INPUTS_ARG_NAME)


def _build_input_tensor_list(ctx: EagerOpExecutionContext) -> List[object]:
    return [ctx.get_input_tensor(index) for index in range(ctx.get_input_num())]


def _inject_ctx_methods(instance, ctx: EagerOpExecutionContext) -> dict:
    original_attrs = {}
    for name in dir(ctx):
        if name.startswith("_"):
            continue
        ctx_method = getattr(ctx, name)
        if not callable(ctx_method):
            continue
        original_attrs[name] = instance.__dict__.get(name, _MISSING)
        setattr(instance, name, ctx_method)
    return original_attrs


def _restore_ctx_methods(instance, original_attrs: dict) -> None:
    for name, value in original_attrs.items():
        if value is _MISSING:
            instance.__dict__.pop(name, None)
        else:
            setattr(instance, name, value)


def _adapt_inputs_execute(method: Callable) -> Callable:
    @wraps(method)
    def wrapper(self, ctx: EagerOpExecutionContext) -> None:
        inputs = _build_input_tensor_list(ctx)
        original_attrs = _inject_ctx_methods(self, ctx)
        try:
            method(self, inputs)
        finally:
            _restore_ctx_methods(self, original_attrs)

    return wrapper


def _build_execute_signature_error(method: Callable) -> str:
    return (
        "EagerExecuteOp.execute only supports execute(self, ctx) or "
        f"execute(self, inputs); got execute{inspect.signature(method)}. "
        "Use 'ctx' for EagerOpExecutionContext or 'inputs' for the input tensor list."
    )


def _positional_params(method: Callable) -> List[inspect.Parameter]:
    return [
        param
        for param in inspect.signature(method).parameters.values()
        if param.kind in _POSITIONAL_PARAMETER_KINDS
    ]


def _has_non_positional_params(method: Callable) -> bool:
    return any(
        param.kind not in _POSITIONAL_PARAMETER_KINDS
        for param in inspect.signature(method).parameters.values()
    )


def _check_execute_arg_count(method: Callable) -> None:
    if len(_positional_params(method)) != 2 or _has_non_positional_params(method):
        raise TypeError(_build_execute_signature_error(method))


def _get_execute_arg_name(method: Callable) -> str:
    _check_execute_arg_count(method)
    arg_name = _positional_params(method)[1].name
    if arg_name not in _SUPPORTED_EXECUTE_ARG_NAMES:
        raise TypeError(_build_execute_signature_error(method))
    return arg_name


class BaseCustomOp(ABC):
    """Base class for Python custom ops."""


class EagerExecuteOp(BaseCustomOp):
    """Base class for Python eager execute custom ops."""

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        method = cls.__dict__.get("execute")
        if callable(method):
            arg_name = _get_execute_arg_name(method)
            if arg_name == _EXECUTE_INPUTS_ARG_NAME:
                cls.execute = _adapt_inputs_execute(method)

    @abstractmethod
    def execute(self, ctx: EagerOpExecutionContext) -> None:
        raise NotImplementedError
