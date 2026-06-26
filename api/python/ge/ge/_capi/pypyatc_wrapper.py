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

import ctypes
import os

from ._lib_loader import load_lib_from_path

LIB_NAME = "libpyatc_wrapper.so"
_dir = os.path.dirname(os.path.abspath(__file__))

# 优先使用 GLOBAL|NOW，确保符号可见并尽早完成重定位
_dlopen_mode = getattr(os, "RTLD_GLOBAL", 0) | getattr(os, "RTLD_NOW", 0)
pyatc_lib = load_lib_from_path(LIB_NAME, _dir, mode=_dlopen_mode)

pyatc_lib.GeApiWrapper_Atc_Main.argtypes = [
    ctypes.c_int,
    ctypes.POINTER(ctypes.c_char_p),
]
pyatc_lib.GeApiWrapper_Atc_Main.restype = ctypes.c_int


def get_pyatc_lib():
    """Get the pyatc wrapper library handle.

    Returns:
        ctypes.CDLL: The loaded libpyatc_wrapper.so library handle.
    """
    return pyatc_lib


def is_pyatc_lib_loaded():
    """Check if pyatc library is loaded.

    Returns:
        bool: True if library is loaded successfully.
    """
    return pyatc_lib is not None
