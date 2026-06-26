#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -------------------------------------------------------------------
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

"""GE error helpers."""

from ge._capi.pygeapi_wrapper import geapi_lib


class GeError(RuntimeError):
    """Runtime error raised for GE C API failures."""

    def __init__(
        self,
        message: str,
        error_message: str = None,
        api_name: str = None,
        context: dict = None,
    ) -> None:
        super().__init__(message)
        self.error_message = error_message
        self.api_name = api_name
        self.context = context or {}


def _decode_c_string(value) -> str:
    if not value:
        return ""
    return value.decode("utf-8", errors="replace")


def get_ge_error_msg() -> str:
    return _decode_c_string(geapi_lib.GeApiWrapper_GEGetErrorMsg())


def raise_ge_error(api_name: str, status: int = None, **context) -> None:
    del status
    error_message = get_ge_error_msg()
    context_text = ", ".join(f"{key}={value}" for key, value in context.items())

    message_parts = [f"{api_name} failed"]
    if context_text:
        message_parts.append(context_text)
    if error_message:
        message_parts.append(error_message)

    raise GeError(
        "; ".join(message_parts),
        error_message=error_message,
        api_name=api_name,
        context=context,
    )
