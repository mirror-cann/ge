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

import ctypes
import os

from ._lib_loader import load_lib_from_path

LIB_NAME = "libge_runtime_wrapper.so"
_dir = os.path.dirname(os.path.abspath(__file__))
tensor_runtime_lib = load_lib_from_path(LIB_NAME, _dir)

c_void_p = ctypes.c_void_p
c_int = ctypes.c_int

tensor_runtime_lib.GeApiWrapper_Tensor_ToHost.restype = c_int
tensor_runtime_lib.GeApiWrapper_Tensor_ToHost.argtypes = [c_void_p]

tensor_runtime_lib.GeApiWrapper_Tensor_ToDevice.restype = c_int
tensor_runtime_lib.GeApiWrapper_Tensor_ToDevice.argtypes = [c_void_p]


def get_tensor_runtime_lib():
    """Get the tensor runtime wrapper library handle.

    Returns:
        ctypes.CDLL: The loaded libge_runtime_wrapper.so library handle.
    """
    return tensor_runtime_lib


def is_tensor_runtime_lib_loaded():
    """Check if tensor runtime library is loaded.

    Returns:
        bool: True if library is loaded successfully.
    """
    return tensor_runtime_lib is not None
