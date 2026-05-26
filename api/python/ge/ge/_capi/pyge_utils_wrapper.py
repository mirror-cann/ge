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

"""ctypes wrapper for GE utility APIs."""

import ctypes
import os

from ._lib_loader import load_lib_from_path

LIB_NAME = "libge_utils_wrapper.so"
_dir = os.path.dirname(os.path.abspath(__file__))
ge_utils_lib = load_lib_from_path(LIB_NAME, _dir)

# 常用C类型别名
c_void_p = ctypes.c_void_p
c_char_ptr = ctypes.POINTER(ctypes.c_char)
c_bool = ctypes.c_bool
c_int64 = ctypes.c_int64
c_size_t = ctypes.c_size_t
c_uint32 = ctypes.c_uint32

# ============ GeUtils C API ============

ge_utils_lib.GeApiWrapper_GeUtils_InferShape.argtypes = [
    c_void_p,
    ctypes.POINTER(c_int64),
    c_size_t,
    ctypes.POINTER(c_size_t),
    c_size_t,
]
ge_utils_lib.GeApiWrapper_GeUtils_InferShape.restype = c_uint32

ge_utils_lib.GeApiWrapper_GeUtils_CheckNodeSupportOnAicore.argtypes = [
    c_void_p,
    ctypes.POINTER(c_bool),
    ctypes.POINTER(c_char_ptr),
]
ge_utils_lib.GeApiWrapper_GeUtils_CheckNodeSupportOnAicore.restype = c_uint32

ge_utils_lib.GeApiWrapper_GeUtils_FreeString.argtypes = [c_char_ptr]
ge_utils_lib.GeApiWrapper_GeUtils_FreeString.restype = None


def get_ge_utils_lib():
    """Get the GE utils wrapper library handle.

    Returns:
        ctypes.CDLL: The loaded libge_utils_wrapper.so library handle.
    """
    return ge_utils_lib


def is_ge_utils_lib_loaded():
    """Check if GE utils library is loaded.

    Returns:
        bool: True if library is loaded successfully.
    """
    return ge_utils_lib is not None
